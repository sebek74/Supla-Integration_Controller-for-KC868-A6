#ifndef TELNETLOG_H
#define TELNETLOG_H

#include <chrono>

class TelnetLog {
  #define LOG_BUFFER_SIZE 65536   // ustaw tak duży jak możliwe

  private:
    inline static NetworkServer telnetServer = NetworkServer(23);
    inline static NetworkClient telnetClient;
    inline static bool telnetInitialized = false;
    inline static bool pause = false;

    inline static uint8_t myLog[LOG_BUFFER_SIZE];
    inline static uint32_t myLogHead = 0, myLogTail = 0;
    inline static uint32_t totalRead = 0, totalWrite = 0;

    inline static uint8_t buffer[1024];
    inline static uint16_t bytesRead = 0;
    inline static uint16_t timeHeaderLen = 0;
  public:
    static void GetLogStatusDescription(char* result, int maxLen) {
      snprintf(result, maxLen, ">>> myLogTail=%d myLogHead=%d totalRead=%d totalWrite=%d", myLogTail, myLogHead, totalRead, totalWrite);
    }
    
    static void OnSerialReceive() {
      if (bytesRead==0) {
        auto now = std::chrono::system_clock::now();
        std::time_t time_now = std::chrono::system_clock::to_time_t(now);
        std::tm* local_time = std::localtime(&time_now);
        auto duration = now.time_since_epoch();
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
        auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration) - std::chrono::duration_cast<std::chrono::milliseconds>(seconds);
        timeHeaderLen = snprintf((char*)buffer, sizeof(buffer), "(%d:%02d:%02d:%03d) ", local_time->tm_hour, local_time->tm_min, local_time->tm_sec, milliseconds.count());
        bytesRead = timeHeaderLen;
      }
      for(;;) {
        for (int bytesAvailable = Serial.available(); bytesAvailable>0; bytesAvailable = Serial.available()) {
          int read = Serial.readBytes(buffer+bytesRead, 1/*sizeof(buffer)-bytesRead*/);
          if (read==0) break;
          //if (buffer[bytesRead] == '\n') continue;
          bytesRead++;
          if (bytesRead==sizeof(buffer)) break;  // bufor pełny nie czytamy więcej
          if (buffer[bytesRead-1] == '\n') break;
        }
        if (bytesRead == 0) return;
        if (buffer[bytesRead-1] != '\n' && bytesRead<sizeof(buffer)) 
          return; // czekamy na koniec linii*/
        if (memcmp(buffer+timeHeaderLen, "SRPC", 4)==0) {
          bytesRead = timeHeaderLen;
          continue;
        }
      
        if (LOG_BUFFER_SIZE-myLogHead==0) {
            myLogHead = 0;
            if (myLogTail==0) myLogTail=1;
        }
        if (bytesRead > LOG_BUFFER_SIZE-myLogHead) {
          int previousHead = myLogHead;
          memcpy(myLog+myLogHead, buffer, LOG_BUFFER_SIZE-myLogHead);
          memcpy(myLog, buffer+LOG_BUFFER_SIZE-myLogHead, bytesRead-LOG_BUFFER_SIZE+myLogHead);
          myLogHead = (myLogHead+bytesRead) % LOG_BUFFER_SIZE;
          if (myLogTail>previousHead || myLogTail<=myLogHead)
            myLogTail = myLogHead+1;
        }
        else {
          memcpy(myLog+myLogHead, buffer, bytesRead);
          myLogHead += bytesRead;
          if (myLogTail<=myLogHead && myLogTail>myLogHead-bytesRead)
            myLogTail = myLogHead+1;
          if (myLogTail>=LOG_BUFFER_SIZE)
            myLogTail = 0;
        }
        totalRead += bytesRead; 
        bytesRead=0;
        timeHeaderLen = 0;
      }
    }

    static void Iterate() { 
      if (!telnetInitialized) {
        telnetServer.begin();
        telnetServer.setNoDelay(true);
        SUPLA_LOG_DEBUG("SERWER TELNET GOTOWY");
        telnetInitialized = true;
      }
      if (telnetInitialized) {
        if (telnetServer.hasClient()) { // czy klient się podłączył
          if (!telnetClient || !telnetClient.connected()) {
            if (telnetClient) telnetClient.stop(); // Rozłącz starego klienta, jeśli wisiał
            telnetClient = telnetServer.available();
            telnetClient.println("--- Połączono z konsolą logów ESP32 przez Telnet ---");
            telnetClient.println("--- ctrl+p, enter = pauza/wznowienie ---");
            telnetClient.println("--- ctrl+i, enter = statystyki loga ---");         
          } else {
            // Odrzuć kolejne połączenie, jeśli jedno jest już aktywne
            telnetServer.available().stop();
          }    
        }
          // Wypychanie danych z bufora RAM do telnet
        if (!pause && myLogHead!=myLogTail && telnetClient && telnetClient.connected()) {
          if (myLogTail>=LOG_BUFFER_SIZE)
            myLogTail=0;
          uint32_t dataToSend = (myLogTail<myLogHead)? myLogHead - myLogTail : LOG_BUFFER_SIZE - myLogTail ;
          telnetClient.write(myLog+myLogTail, dataToSend);
          myLogTail += dataToSend;
          totalWrite += dataToSend;
          if (myLogTail == LOG_BUFFER_SIZE)
            myLogTail = 0;
        }
        // obsługa wejścia
        while (telnetClient && telnetClient.available()) {
          int8_t ch = telnetClient.read();
          if (ch==16) {     // ctrl + p
            pause = !pause;
            telnetClient.println(pause?"PAUZA":"WZNOWIENIE");
          }
          if (ch==9) {     // ctrl + i
            char buf[128];
            GetLogStatusDescription(buf, sizeof (buf));
            telnetClient.println(buf);
          }
        }
      }
    }

    // copy all logs from UART TX to RX
    // and set callback for all RX notifications
    static void Initialize() {
      uart_internal_loopback(0, 3);  
      Serial.onReceive(TelnetLog::OnSerialReceive);  
    }
};

#endif //TELNETLOG_H