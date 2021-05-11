#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESP8266WiFi.h>

class TemperatureSensor
{
public:
  explicit TemperatureSensor(int pin)
    : m_pin(pin)
    , m_oneWire(pin)
    , m_sensors(&m_oneWire)
  {}

  void Begin()
  {
    m_sensors.begin();
  }

  float GetTemperature()
  {
    m_sensors.requestTemperatures();
    return m_sensors.getTempCByIndex(0);
  }

private:
  int m_pin;
  
  OneWire m_oneWire;
  DallasTemperature m_sensors;
};

// ===============================

struct RegisterInfo
{
  RegisterInfo(const String& name, const String& defaultThreshold, const String& id)
    : name(name)
    , defaultThreshold(defaultThreshold)
    , id(id)
  {}
  
  String name;
  String defaultThreshold;
  String id;
};

class WifiClient
{
public:
  enum class SendStatus
  {
    OK,
    FAILED_TO_CONNECT
  };

  void Begin(const char* ssid, const char* password)
  {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED)
    {
      delay(500);
    }

    m_serverAddress = WiFi.gatewayIP();
  }

  // TODO: GET should be SET
  SendStatus Register(const RegisterInfo& info)
  {
    if (m_client.connect(m_serverAddress, 80))
    {
      m_client.print("GET /register?name=");
      m_client.print(info.name);
      m_client.print("&defaultThreshold=");
      m_client.print(info.defaultThreshold);
      m_client.print("&id=");
      m_client.print(info.id);
      m_client.println(" HTTP/1.1");
      m_client.println();
      return SendStatus::OK;
    }
    else
    {
      return SendStatus::FAILED_TO_CONNECT;
    }
  }

  // TODO: GET should be SET
  SendStatus Send(float data, const String& id)
  {
    if (m_client.connect(m_serverAddress, 80))
    {
      m_client.print("GET /update?id=");
      m_client.print(id);
      m_client.print("&data=");
      m_client.print(data);
      m_client.println(" HTTP/1.1");
      m_client.println();
      return SendStatus::OK;
    }
    else
    {
      return SendStatus::FAILED_TO_CONNECT;
    }
  }

private:
  WiFiClient m_client;
  IPAddress m_serverAddress;
};

// ======================================

const int temperaturePin = 4; // GPIO 4, D2

const char* ssid = "ESP32-Access-Point";
const char* password = "123456789";

TemperatureSensor sensor(temperaturePin);
WifiClient client;

String id;

void setup() {
  Serial.begin(115200);
  
  sensor.Begin();

  client.Begin(ssid, password);

  id = String(ESP.getChipId());
  RegisterInfo info("Temperature", "24.0", id);
  client.Register(info);
}

void loop() {
  float temp = sensor.GetTemperature();

  auto status = client.Send(temp, id);
  if (status != WifiClient::SendStatus::OK)
  {
    Serial.println("FAILED TO SEND DATA");
  }
}
