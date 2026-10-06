/**
 * Single-client DNS lookup webserver.
 * Miosix: requires WITH_NETWORKING, NETWORK_WAIT_ONLINE and LWIP_DNS.
 * DHCP supplies DNS servers; static networks must configure DNS in the stack.
 * Linux: g++ -std=c++17 -Wall -Wextra -DHTTP_PORT=8080 main.cpp -o dns-web
 * Open http://<board-ip>/ (or http://localhost:8080/ on Linux).
 *
 * Buffers use 1790 bytes of static storage, not the thread stack. HTML is
 * streamed from constant fragments; no dynamic strings or containers.
 * getaddrinfo() and sockets still allocate internally. Miosix returns one
 * IPv4 address. Synchronous DNS uses the system cache/retry policy and blocks
 * other requests until it completes. Socket I/O has a five-second timeout.
 */
#include <arpa/inet.h>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <netdb.h>
#include <string_view>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

using namespace std::chrono;

#ifndef HTTP_PORT
#define HTTP_PORT 80
#endif

namespace {
constexpr int port = HTTP_PORT;
static_assert(port > 0 && port <= 65535, "Invalid HTTP port");
constexpr size_t maxHeaders = 8192;
constexpr int timeoutSeconds = 5;
char requestBuffer[1024];
char hostname[254];
char outputBuffer[512];
using View = std::string_view;

struct Request {
    int status = 200;
    const char *message = nullptr;
    bool lookup = false;
};

Request failure(int status, const char *message) {
    return {status, message, false};
}

int hexDigit(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

bool decodeHostname(View value) {
    size_t length = 0;
    for (size_t i = 0; i < value.size(); ++i) {
        char c = value[i];
        if (c == '%') {
            if (i + 2 >= value.size())
                return false;
            const int high = hexDigit(value[i + 1]),
                      low = hexDigit(value[i + 2]);
            if (high < 0 || low < 0)
                return false;
            c = static_cast<char>((high << 4) | low);
            i += 2;
        } else if (c == '+')
            c = ' ';
        // Strip a final root dot without needing an extra byte of storage.
        if (c == '.' && i + 1 == value.size())
            break;
        const bool alphanumeric = (c >= 'a' && c <= 'z') ||
                                  (c >= 'A' && c <= 'Z') ||
                                  (c >= '0' && c <= '9');
        if ((!alphanumeric && c != '-' && c != '.') ||
            length == sizeof(hostname) - 1)
            return false;
        hostname[length++] = c;
    }
    hostname[length] = '\0';
    if (!length)
        return false;
    size_t labelLength = 0;
    for (size_t i = 0; i <= length; ++i) {
        if (i == length || hostname[i] == '.') {
            if (!labelLength || hostname[i - 1] == '-')
                return false;
            labelLength = 0;
        } else {
            if (!labelLength && hostname[i] == '-')
                return false;
            if (++labelLength > 63)
                return false;
        }
    }
    return true;
}

Request parseRequestLine(View line) {
    for (unsigned char c : line)
        if ((c <= 0x20 && c != ' ') || c >= 0x7f)
            return failure(400, "Malformed request line.");
    const size_t first = line.find(' ');
    const size_t second =
        first == View::npos ? View::npos : line.find(' ', first + 1);
    if (first == View::npos || second == View::npos || first == 0)
        return failure(400, "Malformed request line.");
    const View version = line.substr(second + 1);
    if (version != "HTTP/1.0" && version != "HTTP/1.1")
        return failure(400, "Expected HTTP/1.0 or HTTP/1.1.");
    if (line.substr(0, first) != "GET")
        return failure(405, "Only GET is supported.");
    const View target = line.substr(first + 1, second - first - 1);
    if (target.empty() || target.front() != '/')
        return failure(400, "Invalid request target.");
    const size_t question = target.find('?');
    if (target.substr(0, question) != "/")
        return failure(404, "Page not found.");
    if (question == View::npos || question + 1 == target.size())
        return {};
    const View query = target.substr(question + 1);
    // The form has exactly one parameter; reject duplicates and other fields.
    if (query.substr(0, 5) != "host=" || query.find('&') != View::npos)
        return failure(400, "Expected one host parameter.");
    if (!decodeHostname(query.substr(5))) {
        hostname[0] = '\0';
        return failure(400, "Enter a hostname of up to 253 characters, such as "
                            "example.com; omit URLs and paths.");
    }
    return {200, nullptr, true};
}

ssize_t receive(int fd, char *buffer, size_t size) {
    ssize_t count;
    do {
        count = recv(fd, buffer, size, 0);
    } while (count < 0 && errno == EINTR);
    return count;
}

Request receiveFailure(ssize_t count) {
    if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        return failure(408, "Request timed out.");
    return failure(400, "Incomplete request.");
}

Request readRequest(int fd) {
    hostname[0] = '\0';
    size_t used = 0, lineEnd;
    for (;;) {
        const ssize_t count =
            receive(fd, requestBuffer + used, sizeof(requestBuffer) - used);
        if (count <= 0)
            return receiveFailure(count);
        used += static_cast<size_t>(count);
        lineEnd = View(requestBuffer, used).find("\r\n");
        if (lineEnd != View::npos)
            break;
        if (used == sizeof(requestBuffer))
            return failure(414, "Request line too long.");
    }
    Request request = parseRequestLine(View(requestBuffer, lineEnd));
    // Reuse the buffer to discard headers, keeping delimiter state across
    // reads.
    size_t total = lineEnd + 2, position = lineEnd + 2;
    unsigned matched = 2; // The request line already ended with CRLF.
    constexpr char endHeaders[] = "\r\n\r\n";
    for (;;) {
        for (; position < used; ++position) {
            if (++total > maxHeaders)
                return failure(431, "Request headers too large.");
            const char c = requestBuffer[position];
            if (c == '\0')
                return failure(400, "Malformed headers.");
            matched =
                c == endHeaders[matched] ? matched + 1 : (c == '\r' ? 1 : 0);
            if (matched == 4)
                return request;
        }
        const ssize_t count = receive(fd, requestBuffer, sizeof(requestBuffer));
        if (count <= 0)
            return receiveFailure(count);
        used = static_cast<size_t>(count);
        position = 0;
    }
}

// Stage output to avoid sending each HTML fragment as a tiny write.
class Writer {
  public:
    explicit Writer(int fd) : fd(fd) {}
    void append(View text) {
        while (good && !text.empty()) {
            size_t amount = sizeof(outputBuffer) - used;
            if (amount > text.size())
                amount = text.size();
            std::memcpy(outputBuffer + used, text.data(), amount);
            used += amount;
            text.remove_prefix(amount);
            if (used == sizeof(outputBuffer))
                flush();
        }
    }
    void escaped(View text) {
        for (char c : text) {
            switch (c) {
            case '&':
                append("&amp;");
                break;
            case '<':
                append("&lt;");
                break;
            case '>':
                append("&gt;");
                break;
            case '"':
                append("&quot;");
                break;
            case '\'':
                append("&#39;");
                break;
            default:
                append(View(&c, 1));
                break;
            }
        }
    }
    void flush() {
        size_t sent = 0;
        while (good && sent < used) {
            // Both Linux and Miosix accept MSG_NOSIGNAL. Miosix has no SIGPIPE.
            const ssize_t count =
                send(fd, outputBuffer + sent, used - sent, MSG_NOSIGNAL);
            if (count < 0 && errno == EINTR)
                continue;
            if (count <= 0)
                good = false;
            else
                sent += static_cast<size_t>(count);
        }
        used = 0;
    }

  private:
    int fd;
    size_t used = 0;
    bool good = true;
};

const char *statusLine(int status) {
    switch (status) {
    case 400:
        return "400 Bad Request";
    case 404:
        return "404 Not Found";
    case 405:
        return "405 Method Not Allowed";
    case 408:
        return "408 Request Timeout";
    case 414:
        return "414 URI Too Long";
    case 431:
        return "431 Request Header Fields Too Large";
    default:
        return "200 OK";
    }
}

void respond(int fd, Request request) {
    char address[INET_ADDRSTRLEN] = {};
    long long lookupMs = 0;
    const char *message = request.message;
    if (request.lookup) {
        addrinfo hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        addrinfo *result = nullptr;
        const auto start = steady_clock::now();
        const int error = getaddrinfo(hostname, nullptr, &hints, &result);
        lookupMs =
            duration_cast<milliseconds>(steady_clock::now() - start).count();
        if (error)
            message = gai_strerror(error);
        else if (!result || !result->ai_addr || result->ai_family != AF_INET ||
                 result->ai_addrlen < sizeof(sockaddr_in) ||
                 !inet_ntop(AF_INET,
                            &reinterpret_cast<sockaddr_in *>(result->ai_addr)
                                 ->sin_addr,
                            address, sizeof(address)))
            message = "No usable IPv4 address was returned.";
        if (result)
            freeaddrinfo(result);
    }

    Writer writer(fd);
    writer.append("HTTP/1.0 ");
    writer.append(statusLine(request.status));
    writer.append("\r\nContent-Type: text/html; charset=utf-8\r\n"
                  "Connection: close\r\nCache-Control: no-store\r\n");
    if (request.status == 405)
        writer.append("Allow: GET\r\n");
    writer.append(
        "\r\n<!doctype html><html lang=\"en\"><head>"
        "<meta charset=\"utf-8\"><meta name=\"viewport\" "
        "content=\"width=device-width\">"
        "<title>DNS lookup</title></head><body><h1>DNS lookup</h1>"
        "<form method=\"get\" action=\"/\"><label "
        "for=\"host\">Hostname</label> "
        "<input id=\"host\" name=\"host\" type=\"text\" maxlength=\"254\" "
        "required placeholder=\"example.com\" value=\"");
    writer.escaped(hostname);
    writer.append("\"> <button type=\"submit\">Lookup</button></form>"
                  "<p>Enter a hostname to find an IPv4 address.</p>");
    if (message) {
        writer.append("<p role=\"status\">Error: ");
        writer.escaped(message);
        writer.append("</p>");
    }
    if (request.lookup) {
        writer.append("<pre role=\"status\">");
        if (!message) {
            writer.append("Name: ");
            writer.escaped(hostname);
            writer.append("\nIPv4 address: ");
            writer.escaped(address);
            writer.append("\n");
        }
        // Integer conversion avoids pulling in snprintf's larger formatter.
        char timing[24];
        const auto converted =
            std::to_chars(timing, timing + sizeof(timing), lookupMs);
        writer.append("Lookup time: ");
        writer.append(View(timing, converted.ptr - timing));
        writer.append(" ms");
        writer.append("</pre>");
    }
    writer.append("</body></html>\n");
    writer.flush();
}
} // namespace

int main() {
    const int server = socket(AF_INET, SOCK_STREAM, 0);
    if (server < 0) {
        std::perror("socket");
        return 1;
    }
    int reuse = 1;
    if (setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) <
        0) {
        std::perror("SO_REUSEADDR");
        close(server);
        return 1;
    }
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);
    if (bind(server, reinterpret_cast<sockaddr *>(&address), sizeof(address)) <
            0 ||
        listen(server, 2) < 0) {
        std::perror("bind/listen");
        close(server);
        return 1;
    }
    std::printf("DNS lookup server listening on port %d\n", port);
    for (;;) {
        const int client = accept(server, nullptr, nullptr);
        if (client < 0) {
            if (errno == EINTR)
                continue;
            std::perror("accept");
            sleep(1); // Avoid spinning on persistent resource exhaustion.
            continue;
        }
        timeval timeout{timeoutSeconds, 0};
        if (setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &timeout,
                       sizeof(timeout)) < 0 ||
            setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, &timeout,
                       sizeof(timeout)) < 0) {
            std::perror("client timeout");
        } else
            respond(client, readRequest(client));
        close(client);
    }
}
