#pragma once

class WebServer;

// Reading-stats JSON and CSV endpoints of the web server (docs/webserver-endpoints.md). Every
// reply is streamed in chunks from the stats files; nothing is kept between requests.
namespace statsweb {

void handleSummary(WebServer& server);      // GET /api/stats/summary
void handleGrid(WebServer& server);         // GET /api/stats/grid?range=30|365|all
void handleBooks(WebServer& server);        // GET /api/stats/books
void handleBook(WebServer& server);         // GET /api/stats/book?book=N
void handleSessions(WebServer& server);     // GET /api/stats/sessions[?book=N]
void handleHooks(WebServer& server);        // GET /api/stats/hooks?book=N
void handleRecords(WebServer& server);      // GET /api/stats/records
void handleYear(WebServer& server);         // GET /api/stats/year?year=Y
void handleSpeed(WebServer& server);        // GET /api/stats/speed
void handleSessionsCsv(WebServer& server);  // GET /api/stats/sessions.csv
void handleBooksCsv(WebServer& server);     // GET /api/stats/books.csv

}  // namespace statsweb
