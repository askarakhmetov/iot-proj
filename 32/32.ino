#include <WiFi.h>

#include <vector>
#include <utility>
#include <algorithm>

struct RequestData
{
  String path;
  std::vector<std::pair<String, String>> attributes;
};

RequestData ParseHTML(const String& request)
{
  RequestData data;

  int pathStart = request.indexOf('/');
  int endIndex = request.indexOf('?', pathStart);
  if (endIndex == -1)
  {
    endIndex = request.indexOf(' ', pathStart);
  }

  data.path = request.substring(pathStart, endIndex);

  while (request[endIndex] != ' ')
  {
    int nameStart = endIndex + 1;
    int nameEnd = request.indexOf('=', nameStart);
    int valueStart = nameEnd + 1;
    endIndex = request.indexOf('&', valueStart);
  
    if (endIndex == -1)
    {
      endIndex = request.indexOf(' ', valueStart);
    }
  
    String name = request.substring(nameStart, nameEnd);
    String value = request.substring(valueStart, endIndex);

    data.attributes.push_back({ name, value });
  }
  
  return data;
}

class WifiServer
{
public:
  WifiServer(int port)
    : m_server(port)
  {}

  void Begin(const char* ssid, const char* password)
  {
    WiFi.softAP(ssid, password);

    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);
    
    m_server.begin();
  }

  bool EstablishConnection()
  {
    m_client = m_server.available();
    return m_client;
  }

  RequestData GetData()
  {
    String header = "";
    String currentLine = "";

    RequestData data;
    
    while (m_client.connected())
    {
      if (m_client.available())
      {
        char c = m_client.read();
        header += c;

        if (c == '\n')
        {
          if (currentLine.length() != 0)
          {
            currentLine = "";
          }
          else
          {
            data = ParseHTML(header);

            CloseConnection();
            
            break;
          }
        }
        else if (c != '\r')
        {
          currentLine += c;
        }
      }
    }

    return data;
  }

  IPAddress GetClientIP()
  {
    return m_client.remoteIP();    
  }

private:
  WiFiServer m_server;
  WiFiClient m_client; // TODO: find out how to support multiple connections

  void CloseConnection()
  {
    m_client.println("HTTP/1.1 200 OK");
    m_client.println("Content-type: text/plain");
    m_client.println("Connection: close");
    m_client.println();
    m_client.println();

    m_client.stop();
  }
};

// ==============================================

#define MAX_CONN 4

struct Device
{
  String name;
  String id;

  float currentValue;

  float threshold;
  float defaultThreshold;

  bool IsOverThreshold() const
  {
    return currentValue >= threshold;
  }
};

std::vector<Device> devices;
int deviceIndex = 0;

// ==============================================

#include <TFT_eSPI.h>
#include <SPI.h>

TFT_eSPI tft = TFT_eSPI();

const int plusPin = 26;
const int minusPin = 27;
const int switchPin = 19;

const int APPin = 17;

const int port = 80;
const char* ssid = "ESP32-Access-Point";
const char* password = "123456789";

WifiServer server(port);

void setup()
{
  Serial.begin(115200);
  server.Begin(ssid, password);
  Serial.print("Starting on port ");
  Serial.println(port);

  pinMode(APPin, OUTPUT);

  pinMode(plusPin, INPUT);
  pinMode(minusPin, INPUT);
  pinMode(switchPin, INPUT);

  digitalWrite(APPin, HIGH);

  tft.init();
  tft.setRotation(1);
}

float threshold = 26.0;

bool isOn = false;
bool isOverThreshold = false;

unsigned long previousMillis = 0;
const long interval = 100;

const int kDisplayWidth = 160;
const int kDisplayHeight = 128;

const int kFooterX = 0;
const int kFooterY = 100;
const int kFooterWidth = kDisplayWidth;
const int kFooterHeight = kDisplayHeight - kFooterY;

void loop()
{
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0, 0, 2);

  // Draw footer
  if (!devices.empty())
  {
    tft.drawLine(kFooterX, kFooterY, kFooterX + kFooterWidth, kFooterY, TFT_WHITE);

    const size_t size = devices.size();
    const int rectWidth = kFooterWidth / size;

    for (size_t i = 0; i < size; ++i)
    {
      auto color = i == deviceIndex ? TFT_BLUE : TFT_WHITE;

      if (devices[i].IsOverThreshold() && isOn)
      {
        tft.fillRect(kFooterX + i * rectWidth, kFooterY, rectWidth, kFooterHeight, TFT_YELLOW);
      }
      else
      {
        tft.drawRect(kFooterX + i * rectWidth, kFooterY, rectWidth, kFooterHeight, color);
  
        char c = devices[i].name[0];
  
        tft.setTextColor(color, TFT_BLACK);
        tft.drawCentreString(String(c), kFooterX + i * rectWidth + rectWidth / 2, kFooterY + kFooterHeight / 4, 1);
      }
    }
  }

  if (devices.empty())
  {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setTextSize(2);
    tft.println("No devices");
  }
  else
  {
    auto& device = devices[deviceIndex];

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.println(device.name);
    tft.println(device.threshold);
    tft.println(device.currentValue);
  }
  
  if (server.EstablishConnection())
  {
    auto clientIp = server.GetClientIP();
    Serial.println(clientIp);
    auto data = server.GetData();
    
    if (data.path == "/register")
    {
      Device newDevice;
      
      auto& attributes = data.attributes;

      // TODO: ip address
      newDevice.name = attributes[0].second;
      newDevice.defaultThreshold = attributes[1].second.toFloat();
      newDevice.threshold = newDevice.defaultThreshold;
      newDevice.id = attributes[2].second;

      auto it = std::find_if(devices.begin(), devices.end(), [&newDevice](const Device& device)
      {
        return device.id == newDevice.id;
      });

      if (it != devices.end())
      {
        it->name = newDevice.name;
        it->defaultThreshold = newDevice.defaultThreshold;
      }
      else
      {
        devices.push_back(newDevice);
      }
    }
    else if (data.path == "/update")
    {
      auto& attributes = data.attributes;
      
      String id = attributes[0].second;
      float value = attributes[1].second.toFloat();

      auto it = std::find_if(devices.begin(), devices.end(), [&id](const Device& device)
      {
        return device.id == id;
      });

      if (it != devices.end())
      {
        it->currentValue = value;
      }
    }
    
//    float data = server.GetData().toFloat();
//    isOverThreshold = data >= device.threshold;
//    Serial.println(data);
  }


  if (!devices.empty())
  {
    auto& device = devices[deviceIndex];

    if (digitalRead(plusPin))
    {
      device.threshold += 1;
    }

    if (digitalRead(minusPin))
    {
      device.threshold -= 1;
    }

    if (digitalRead(switchPin))
    {
      deviceIndex = (deviceIndex + 1) % devices.size();
    }
  }

  isOn = !isOn;

  delay(100);
}
