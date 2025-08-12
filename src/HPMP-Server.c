#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <arpa/inet.h>
#include <pthread.h>

// Default port for protocol PMP (777) and for the HTTP which is 8080
#define PMP_PORT 777
#define HTTP_PORT 8080

// Defining logging file
#define LOGFILE "pmp-messages.log"

pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;


// Writing log file with the following data: day/month/year - time │ ip │ message [...]
void log_message(const char *ip, const char *message){
  time_t t = time(NULL);
  struct tm *tm_info = localtime(&t);
  char date[64];
  strftime(date, sizeof(date), "%d/%m/%Y - %H:%M:%S", tm_info);

  pthread_mutex_lock(&log_mutex);
  FILE *f = fopen(LOGFILE, "a");
  if (f){
    fprintf(f, "%s │ %s │ Message: %s\n", date, ip, message);
    fclose(f);
  } else {
    perror("fopen");
  }
  pthread_mutex_unlock(&log_mutex);

  printf("[PMP] Received message from %s: %s\n", ip, message);
}

// url decoding for form body (handles + -> space and %xx)
void url_decode(char *s){
  char *dst = s, *src = s;
  while (*src){
    if (*src == '+'){
      *dst++ = ' ';
      src++;
    } else if (*src == '%' && src[1] && src[2]){
      char hex[3] = { src[1], src[2], 0 };
      *dst++ = (char) strtol(hex, NULL, 16);
      src += 3;
    } else{
      *dst++ = *src++;
    }
  }
  *dst = '\0';
}

// HPMP TCP server worker thread
void *pmp_server_thread(void *arg){
  int server_fd, client_fd;
  struct sockaddr_in address;
  socklen_t addrlen = sizeof(address);
  char buffer[2048];

  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1){
    perror("socket");
    return NULL;
  }

  int opt = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(PMP_PORT);

  if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0){
    perror("bind");
    close(server_fd);
    return NULL;
  }

  if (listen(server_fd, 10) < 0){
    perror("listen");
    close(server_fd);
    return NULL;
  }

  printf("[PMP] Server listening on port %d\n", PMP_PORT);

  while (1){
    client_fd = accept(server_fd, (struct sockaddr *)&address, &addrlen);
    if (client_fd < 0){
      perror("accept");
      continue;
    }

    memset(buffer, 0, sizeof(buffer));
    int valread = read(client_fd, buffer, sizeof(buffer) - 1);
    if (valread > 0){
      // trim trailing newlines
      for (int i = valread - 1; i >= 0; --i){
        if (buffer[i] == '\r' || buffer[i] == '\n') buffer[i] = '\0';
        else break;
      }
      char *client_ip = inet_ntoa(address.sin_addr);
      log_message(client_ip, buffer);
    }

    close(client_fd);
  }

  close(server_fd);
  return NULL;
}

// Send a full HTTP response (headers + body)
void send_response(int client_fd, const char *headers, const char *body){
  write(client_fd, headers, strlen(headers));
  if (body) write(client_fd, body, strlen(body));
}

// HTML and js index page
void serve_index(int client_fd){
  const char *html_headers =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/html; charset=utf-8\r\n\r\n";

  const char *html_body =
    "<!doctype html>"
"<html>"
"<head>"
"<meta charset='utf-8'>"
"<title>PMP Web</title>"
"<style>"
"body{background-color:#081228;font-family:Lucida Console;margin:20px;}"
"#log{border:1px solid #ccc;padding:10px;height:300px;width:600px;overflow:auto;background:#354873;white-space:pre-wrap;color:White;font-size:14px;border:0px solid #ccc;}"
"form{margin-bottom:10px;}"
"h1, h2{color:#33FF48;}"
"input::placeholder{color:White;font-family:Lucida Console;}"
"input[type=text]{background-color:#354873;color:white;border:0px solid #ccc;padding:5px;}"
"input[type=submit]{padding:5px 15px;border:0px;background-color:#33FF48;}"
"</style>"
"</head>"
"<body>"
"<h1>⚙️ PMP Web Interface</h1>"
"<form id='msgForm'>"
"<input id='msg' type='text' placeholder='Write a message' style='width:610px;'/>"
"<br><br>"
"<input type='submit' value='Send Message'/>"
"</form>"
"<h2>📄 Live Message Log:</h2>"
"<div id='log'>Loading data...</div>"
"<script>"
"document.getElementById('msgForm').addEventListener('submit', async function(e){"
"  e.preventDefault();"
"  let m = document.getElementById('msg').value.trim();"
"  if (!m) return;"
"  await fetch('/send', {"
"    method: 'POST',"
"    headers: {'Content-Type': 'application/x-www-form-urlencoded'},"
"    body: 'msg=' + encodeURIComponent(m)"
"  });"
"  document.getElementById('msg').value = '';"
"  loadLog();"
"});"
"async function loadLog(){"
"  try {"
"    let r = await fetch('/log');"
"    let t = await r.text();"
"    let lines = t.split('\\n');"
"    let numbered = lines.map((line, i) => (i+1) + ' ― ' + line).join('\\n');"
"    let l = document.getElementById('log');"
"    l.textContent = numbered;"
"    l.scrollTop = l.scrollHeight;"
"  } catch(e){}"
"}"
"setInterval(loadLog, 1000);"
"loadLog();"
"</script>"
"</body></html>";
  send_response(client_fd, html_headers, html_body);
}

// "/log" endpoint containing plain text information about logfile (pmp-message.log)
void serve_log(int client_fd){
  pthread_mutex_lock(&log_mutex);
  FILE *f = fopen(LOGFILE, "r");
  if (!f){
    const char *resp =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/plain; charset=utf-8\r\n\r\n"
      "No log yet.\n";
    pthread_mutex_unlock(&log_mutex);
    send_response(client_fd, resp, NULL);
    return;
  }

  const char *hdr =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/plain; charset=utf-8\r\n\r\n";
  write(client_fd, hdr, strlen(hdr));

  char line[1024];
  while (fgets(line, sizeof(line), f)){
    write(client_fd, line, strlen(line));
  }

  fclose(f);
  pthread_mutex_unlock(&log_mutex);
}

// Extracts POST body (very small/simple parser)
char *get_body(char *req){
  char *p = strstr(req, "\r\n\r\n");
  if (!p) return NULL;
  return p + 4;
}

// HTTP server thread
void *http_server_thread(void *arg){
  int server_fd, client_fd;
  struct sockaddr_in address;
  socklen_t addrlen = sizeof(address);
  char buffer[8192];

  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1){
    perror("socket");
    return NULL;
  }

  int opt = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(HTTP_PORT);

  if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0){
    perror("bind");
    close(server_fd);
    return NULL;
  }

  if (listen(server_fd, 10) < 0){
    perror("listen");
    close(server_fd);
    return NULL;
  }

  printf("[HTTP] Server listening on port %d\n", HTTP_PORT);

  while (1){
    client_fd = accept(server_fd, (struct sockaddr *)&address, &addrlen);
    if (client_fd < 0){
      perror("accept");
      continue;
    }

    memset(buffer, 0, sizeof(buffer));
    int valread = read(client_fd, buffer, sizeof(buffer) - 1);
    if (valread <= 0){
      close(client_fd);
      continue;
    }

    // doing simple routing
    if (strncmp(buffer, "GET / ", 6) == 0 || strncmp(buffer, "GET /HTTP", 8) == 0){
      serve_index(client_fd);
    } else if (strncmp(buffer, "GET /log", 8) == 0){ // log endpoint containing plaintextt data
      serve_log(client_fd);
    } else if (strncmp(buffer, "POST /send", 10) == 0){
      char *body = get_body(buffer);
      if (body && strncmp(body, "msg=", 4) == 0){
        char *msg = body + 4;
        // copy because url_decode modifies in-place and body points into buffer
        char tmp[2048];
        strncpy(tmp, msg, sizeof(tmp)-1);
        tmp[sizeof(tmp)-1] = '\0';
        url_decode(tmp);
        // using remote address from accept() as client ip representation
        char *client_ip = inet_ntoa(address.sin_addr);
        log_message(client_ip, tmp);
      }
      const char *resp = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nOK";
      write(client_fd, resp, strlen(resp));
    } else {
      const char *resp = "HTTP/1.1 404 Not Found\r\n\r\n";
      write(client_fd, resp, strlen(resp));
    }
    close(client_fd);
  }
  close(server_fd);
  return NULL;
}

int main(int argc, char *argv[]){
  pthread_t t1, t2;

  // create threads for PMP server and HTTP server
  if (pthread_create(&t1, NULL, pmp_server_thread, NULL) != 0){
    perror("pthread_create pmp");
    return 1;
  }
  if (pthread_create(&t2, NULL, http_server_thread, NULL) != 0){
    perror("pthread_create http");
    return 1;
  }

  // join (wait) on both threads (program runs until killed)
  pthread_join(t1, NULL);
  pthread_join(t2, NULL);
  return 0;
}
