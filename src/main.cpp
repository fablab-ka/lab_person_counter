#include <ESP8266WiFi.h>
#include <MD_MAX72xx.h>
#include <SPI.h>

#define DEBUG 0

#if DEBUG
#define	PRINT(s, v)	{ Serial.print(F(s)); Serial.print(v); }
#define PRINTS(s)   { Serial.print(F(s)); }
#else
#define	PRINT(s, v)
#define PRINTS(s)
#endif


#define	MAX_DEVICES	4

#define	CLK_PIN		D5 // or SCK
#define	DATA_PIN	D7 // or MOSI
#define	CS_PIN		D8 // or SS

#define	INCREASE_BUTTON_PIN		D4
#define	DECREASE_BUTTON_PIN		D3

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

const char* ssid = "matrix";
const char* password = "einlangesundtollespasswort";

WiFiServer server(80);

const uint8_t MESG_SIZE = 255;
const uint8_t CHAR_SPACING = 1;
const uint8_t SCROLL_DELAY = 75;
const uint32_t POLL_TIMEOUT = 10000;
const int MAX_NUMBER_OF_PEOPLE = 6;
const char MAX_NUMBER_OF_PEOPLE_CHAR = '6';

char curMessage[MESG_SIZE];
char newMessage[MESG_SIZE];
bool newMessageAvailable = false;

int currentNumberOfPeople = 0;

uint32_t lastPollTime = 0;
bool isShowingIP = false;

int increaseButtonState;
int decreaseButtonState;
int lastIncreaseButtonState = HIGH;
int lastDecreaseButtonState = HIGH;

unsigned long lastIncreaseButtonDebounceTime = 0;
unsigned long lastDecreaseButtonDebounceTime = 0;
unsigned long debounceDelay = 50;


char WebResponse[] = "HTTP/1.1 200 OK\nContent-Type: text/html\n\n";

char WebPage[] =
"<!DOCTYPE html>" \
"<html>" \
"<head>" \
"<title>eTechPath MAX7219 ESP8266</title>" \
"<style>" \
"html, body" \
"{" \
"width: 600px;" \
"height: 400px;" \
"margin: 0px;" \
"border: 0px;" \
"padding: 10px;" \
"background-color: white;" \
"}" \
"#container " \
"{" \
"width: 100%;" \
"height: 100%;" \
"margin-left: 200px;" \
"border: solid 2px;" \
"padding: 10px;" \
"background-color: #b3cbf2;" \
"}" \
"</style>"\
"<script>" \
"strLine = \"\";" \
"function SendText()" \
"{" \
"  nocache = \"/&nocache=\" + Math.random() * 1000000;" \
"  var request = new XMLHttpRequest();" \
"  strLine = \"&MSG=\" + document.getElementById(\"txt_form\").Message.value;" \
"  request.open(\"GET\", strLine + nocache, false);" \
"  request.send(null);" \
"}" \
"</script>" \
"</head>" \
"<body>" \
"<div id=\"container\">"\
"<H1><b>WiFi MAX7219 LED Matrix Display</b></H1>" \
"<form id=\"txt_form\" name=\"frmText\">" \
"<label>Msg:<input type=\"text\" name=\"Message\" maxlength=\"255\"></label><br><br>" \
"</form>" \
"<br>" \
"<input type=\"submit\" value=\"Send Text\" onclick=\"SendText()\">" \
"</div>" \
"</body>" \
"</html>";

String err2Str(wl_status_t code)
{
  switch (code)
  {
  case WL_IDLE_STATUS:    return("IDLE");           break; // WiFi is in process of changing between statuses
  case WL_NO_SSID_AVAIL:  return("NO_SSID_AVAIL");  break; // case configured SSID cannot be reached
  case WL_CONNECTED:      return("CONNECTED");      break; // successful connection is established
  case WL_CONNECT_FAILED: return("CONNECT_FAILED"); break; // password is incorrect
  case WL_DISCONNECTED:   return("CONNECT_FAILED"); break; // module is not configured in station mode
  default: return("??");
  }
}

uint8_t htoi(char c)
{
  c = toupper(c);
  if ((c >= '0') && (c <= '9')) return(c - '0');
  if ((c >= 'A') && (c <= 'F')) return(c - 'A' + 0xa);
  return(0);
}

boolean getText(char *szMesg, char *psz, uint8_t len)
{
  boolean isValid = false;  // text received flag
  char *pStart, *pEnd;      // pointer to start and end of text

  // get pointer to the beginning of the text
  pStart = strstr(szMesg, "/&MSG=");

  if (pStart != NULL)
  {
    pStart += 6;  // skip to start of data
    pEnd = strstr(pStart, "/&");

    if (pEnd != NULL)
    {
      while (pStart != pEnd)
      {
        if ((*pStart == '%') && isdigit(*(pStart+1)))
        {
          // replace %xx hex code with the ASCII character
          char c = 0;
          pStart++;
          c += (htoi(*pStart++) << 4);
          c += htoi(*pStart++);
          *psz++ = c;
        }
        else
          *psz++ = *pStart++;
      }

      *psz = '\0'; // terminate the string
      isValid = true;
    }
  }

  return(isValid);
}

void showNumberOfPeople() {
  mx.clear();

  sprintf(curMessage, "%d", currentNumberOfPeople);
  mx.setChar(24, curMessage[0]);
  mx.setChar(17, '/');
  mx.setChar(10, MAX_NUMBER_OF_PEOPLE_CHAR);
  isShowingIP = false;
}

void showIP() {
  mx.clear();
  sprintf(curMessage, "%03d:%03d:%03d:%03d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
  isShowingIP = true;
}

void handleWiFi(void)
{
  static enum { S_IDLE, S_WAIT_CONN, S_READ, S_EXTRACT, S_RESPONSE, S_DISCONN } state = S_IDLE;
  static char szBuf[1024];
  static uint16_t idxBuf = 0;
  static WiFiClient client;
  static uint32_t timeStart;

  switch (state)
  {
  case S_IDLE:   // initialise
    PRINTS("\nS_IDLE");
    idxBuf = 0;
    state = S_WAIT_CONN;
    break;

  case S_WAIT_CONN:   // waiting for connection
    {
      client = server.available();
      if (!client) break;
      if (!client.connected()) break;

#if DEBUG
      char szTxt[20];
      sprintf(szTxt, "%03d:%03d:%03d:%03d", client.remoteIP()[0], client.remoteIP()[1], client.remoteIP()[2], client.remoteIP()[3]);
      PRINT("\nNew client @ ", szTxt);
#endif

      timeStart = millis();
      state = S_READ;
    }
    break;

  case S_READ: // get the first line of data
    PRINTS("\nS_READ");
    while (client.available())
    {
      char c = client.read();
      if ((c == '\r') || (c == '\n'))
      {
        szBuf[idxBuf] = '\0';
        client.flush();
        PRINT("\nRecv: ", szBuf);
        state = S_EXTRACT;
      }
      else
        szBuf[idxBuf++] = (char)c;
    }
    if (millis() - timeStart > 1000)
    {
      PRINTS("\nWait timeout");
      state = S_DISCONN;
    }
    break;


  case S_EXTRACT: // extract data
    PRINTS("\nS_EXTRACT");
    // Extract the string from the message if there is one
    newMessageAvailable = getText(szBuf, newMessage, MESG_SIZE);
    PRINT("\nNew Msg: ", newMessage);
    state = S_RESPONSE;
    break;

  case S_RESPONSE: // send the response to the client
    PRINTS("\nS_RESPONSE");
    // Return the response to the client (web page)
    client.print(WebResponse);
    client.print(WebPage);
    state = S_DISCONN;

    // show number of people if display is polled
    lastPollTime = millis();
    showNumberOfPeople();
    break;

  case S_DISCONN: // disconnect client
    PRINTS("\nS_DISCONN");
    client.flush();
    client.stop();
    state = S_IDLE;
    break;

  default:  state = S_IDLE;
  }
}

uint8_t scrollDataSource(uint8_t dev, MD_MAX72XX::transformType_t t)
// Callback function for data that is required for scrolling into the display
{
  static enum { S_IDLE, S_NEXT_CHAR, S_SHOW_CHAR, S_SHOW_SPACE } state = S_IDLE;
  static char		*p;
  static uint16_t	curLen, showLen;
  static uint8_t	cBuf[8];
  uint8_t colData = 0;

  // finite state machine to control what we do on the callback
  switch (state)
  {
  case S_IDLE: // reset the message pointer and check for new message to load
    PRINTS("\nS_IDLE");
    p = curMessage;      // reset the pointer to start of message
    if (newMessageAvailable)  // there is a new message waiting
    {
      strcpy(curMessage, newMessage); // copy it in
      newMessageAvailable = false;
    }
    state = S_NEXT_CHAR;
    break;

  case S_NEXT_CHAR: // Load the next character from the font table
    PRINTS("\nS_NEXT_CHAR");
    if (*p == '\0')
      state = S_IDLE;
    else
    {
      showLen = mx.getChar(*p++, sizeof(cBuf) / sizeof(cBuf[0]), cBuf);
      curLen = 0;
      state = S_SHOW_CHAR;
    }
    break;

  case S_SHOW_CHAR:	// display the next part of the character
    PRINTS("\nS_SHOW_CHAR");
    colData = cBuf[curLen++];
    if (curLen < showLen)
      break;

    // set up the inter character spacing
    showLen = (*p != '\0' ? CHAR_SPACING : (MAX_DEVICES*COL_SIZE)/2);
    curLen = 0;
    state = S_SHOW_SPACE;
    // fall through

  case S_SHOW_SPACE:	// display inter-character spacing (blank column)
    PRINT("\nS_ICSPACE: ", curLen);
    PRINT("/", showLen);
    curLen++;
    if (curLen == showLen)
      state = S_NEXT_CHAR;
    break;

  default:
    state = S_IDLE;
  }

  return(colData);
}

void scrollText(void)
{
  static uint32_t	prevTime = 0;

  // Is it time to scroll the text?
  if (millis() - prevTime >= SCROLL_DELAY)
  {
    mx.transform(MD_MAX72XX::TSL);	// scroll along - the callback will load all the data
    prevTime = millis();			      // starting point for next time
  }
}

void increaseNumberOfPeople() {
  Serial.println("increaseNumberOfPeople");
  currentNumberOfPeople++;
  if (currentNumberOfPeople > MAX_NUMBER_OF_PEOPLE) {
    currentNumberOfPeople = MAX_NUMBER_OF_PEOPLE;
  }
  showNumberOfPeople();
}

void decreaseNumberOfPeople() {
  Serial.println("decreaseNumberOfPeople");
  currentNumberOfPeople--;
  if (currentNumberOfPeople < 0) {
    currentNumberOfPeople = 0;
  }
  showNumberOfPeople();
}

void handleButtons() {
  int increaseButtonReading = digitalRead(INCREASE_BUTTON_PIN);
  int decreaseButtonReading = digitalRead(DECREASE_BUTTON_PIN);

  if (increaseButtonReading != lastIncreaseButtonState) {
    lastIncreaseButtonDebounceTime = millis();
  }
  if (decreaseButtonReading != lastDecreaseButtonState) {
    lastDecreaseButtonDebounceTime = millis();
  }

  if ((millis() - lastIncreaseButtonDebounceTime) > debounceDelay) {
    if (increaseButtonReading != increaseButtonState) {
      increaseButtonState = increaseButtonReading;

      if (increaseButtonState == LOW) {
        increaseNumberOfPeople();
      }
    }
  }

  if ((millis() - lastDecreaseButtonDebounceTime) > debounceDelay) {
    if (decreaseButtonReading != decreaseButtonState) {
      decreaseButtonState = decreaseButtonReading;

      if (decreaseButtonState == LOW) {
        decreaseNumberOfPeople();
      }
    }
  }

  lastIncreaseButtonState = increaseButtonReading;
  lastDecreaseButtonState = decreaseButtonReading;
}

void setup()
{
  Serial.begin(115200);
#if DEBUG
  PRINTS("\n[MD_MAX72XX WiFi Message Display]\nType a message for the scrolling display from your internet browser");
#endif
  
  pinMode(INCREASE_BUTTON_PIN, INPUT);
  pinMode(DECREASE_BUTTON_PIN, INPUT);

  // Display initialisation
  mx.begin();
  mx.setShiftDataInCallback(scrollDataSource);

  curMessage[0] = newMessage[0] = '\0';

  // Connect to and initialise WiFi network
  PRINT("\nConnecting to ", ssid);
  sprintf(curMessage, "Connecting to %s", ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    PRINT("\n", err2Str(WiFi.status()));
    scrollText();

    delay(50);
  }
  PRINTS("\nWiFi connected");

  // Start the server
  server.begin();
  PRINTS("\nServer started");

  showIP();
  PRINT("\nAssigned IP ", curMessage);
}

void loop()
{
  handleWiFi();

  handleButtons();

  if (isShowingIP) {
    scrollText();
  } else if ((millis() - lastPollTime) >= POLL_TIMEOUT) {
    showIP();
  }
}


