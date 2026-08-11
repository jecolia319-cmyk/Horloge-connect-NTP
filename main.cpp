#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <time.h>
#include <TM1637Display.h>
#include <RtcDS1302.h>
#include <ESP8266HTTPClient.h>
#include <UrlEncode.h>

// --- IDENTIFIANTS WI-FI ---
const char* ssid     = "";
const char* password = "";
const String APP_ID  = "********";
const String APP_KEY = "********";
const String server  = "http://iot.youpilab.com/api";

// --- CONFIGURATION NTP ---
const long gmtOffset_sec = 3600;      // UTC+1
const int daylightOffset_sec = 0;

// --- BROCHES ESP8266 (Sécurisées sans conflit) ---
#define DISPLAY_CLK D1
#define DISPLAY_DIO D2
#define BTN_HEURE   D3
#define BTN_MINUTE  D4
#define BTN_STOP    3   // Broche RX (GPIO 3) pour éviter tout risque sur S3/Flash

// Broches RTC DS1302
#define RTC_RST     D7
#define RTC_CLK     D5
#define RTC_DAT     D6

// Buzzer & LED Unique
#define BUZZER_PIN  D0
#define LED_ROUGE   D8

TM1637Display display(DISPLAY_CLK, DISPLAY_DIO);
ThreeWire myWire(RTC_DAT, RTC_CLK, RTC_RST);
RtcDS1302<ThreeWire> Rtc(myWire);

int currentHour = 0, currentMinute = 0, currentSecond = 0;
int currentDay = 1, currentMonth = 1;
int alarmeHeure = 7, alarmeMinute = 0;

bool alarmeArmee = true;
bool alarmeEnCours = false;
bool alarmeDeclenchee = false;

unsigned long tempsAffichageAlarme = 0;
unsigned long debutAlarme = 0;
const unsigned long DUREE_SONNERIE_MAX = 5 * 60 * 1000; // 5 minutes

// Notification: envoi si l'alarme n'est pas arrêtée
bool notificationSent = false;
const unsigned long NOTIF_DELAY_MS = 30 * 1000; // 30 secondes

// Resynchronisation périodique
unsigned long dernierMiseAJourNTP = 0;
const unsigned long INTERVALLE_NTP = 6 * 3600 * 1000; // Mettre à jour toutes les 6 heures

void synchroniserNTP() {
  if (WiFi.status() == WL_CONNECTED) {
    configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org", "time.nist.gov");
    Serial.print("Synchronisation NTP en cours");

    time_t now = time(nullptr);
    int retries = 0;

    // Attente d'un timestamp valide (après l'année 2020)
    while (now < 1577836800 && retries < 20) {
      delay(500);
      Serial.print(".");
      now = time(nullptr);
      retries++;
    }

    Serial.println();
    struct tm* timeinfo = localtime(&now);

    if (timeinfo && timeinfo->tm_year > 70) {
      RtcDateTime ntpTime(
        timeinfo->tm_year + 1900,
        timeinfo->tm_mon + 1,
        timeinfo->tm_mday,
        timeinfo->tm_hour,
        timeinfo->tm_min,
        timeinfo->tm_sec
      );
      Rtc.SetDateTime(ntpTime);
      dernierMiseAJourNTP = millis();
      Serial.println(" -> Heure NTP appliquee et enregistree dans le RTC avec succes !");
    } else {
      Serial.println(" -> Echec de la recuperation NTP (Utilisation de l'heure du RTC).");
    }
  }
}

// --- Fonctions extraites pour lisibilité ---
void initHardware() {
  display.setBrightness(0x0a);

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_ROUGE, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  noTone(BUZZER_PIN);
  digitalWrite(LED_ROUGE, HIGH);

  pinMode(BTN_HEURE, INPUT_PULLUP);
  pinMode(BTN_MINUTE, INPUT_PULLUP);
  pinMode(BTN_STOP, INPUT_PULLUP);

  Rtc.Begin();
  if (Rtc.GetIsWriteProtected()) Rtc.SetIsWriteProtected(false);
  if (!Rtc.GetIsRunning()) Rtc.SetIsRunning(true);
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connexion au Wi-Fi");

  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 15000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.println(" -> Wi-Fi connecte avec succes !");
    Serial.print(" -> Adresse IP : ");
    Serial.println(WiFi.localIP());
    synchroniserNTP();
  } else {
    Serial.println();
    Serial.println(" -> Echec de la connexion Wi-Fi. (Verification des identifiants necessaire)");
  }
}

void lireHeureRTC() {
  RtcDateTime now = Rtc.GetDateTime();
  currentHour   = now.Hour();
  currentMinute = now.Minute();
  currentSecond = now.Second();
  currentDay    = now.Day();
  currentMonth  = now.Month();
}

void envoyerNotification() {
  if (notificationSent) return;
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi non connecté: notification non envoyee");
    return;
  }

  String information = "Alarme manquee a " + String(currentHour) + ":" + String(currentMinute) + ":" + String(currentSecond);
  String informationEnc = urlEncode(information);
  String url = server + "/data/send?" +
               "APP_ID="  + APP_ID  +
               "&APP_KEY=" + APP_KEY +
               "&P1="    + informationEnc;

  WiFiClient client;
  HTTPClient http;
  http.begin(client, url);
  int codeRetour = http.GET();
  if (codeRetour > 0) {
    String reponse = http.getString();
    Serial.println("Réponse du serveur :");
    Serial.println(reponse);
    notificationSent = true;
  } else {
    Serial.print("Erreur envoi notif, code: ");
    Serial.println(codeRetour);
  }
  http.end();
}

void gererBoutons() {
  // Bouton STOP
  if (digitalRead(BTN_STOP) == LOW) {
    if (alarmeEnCours) {
      alarmeEnCours = false;
      noTone(BUZZER_PIN);
      if (!notificationSent && millis() - debutAlarme >= NOTIF_DELAY_MS) {
        envoyerNotification();
      }
    }
    alarmeDeclenchee = false;
    delay(200);
    return;
  }

  // Reglage heure
  if (digitalRead(BTN_HEURE) == LOW) {
    alarmeHeure = (alarmeHeure + 1) % 24;
    tempsAffichageAlarme = millis();
    alarmeArmee = true;
    alarmeDeclenchee = false;
    delay(200);
    return;
  }

  // Reglage minute
  if (digitalRead(BTN_MINUTE) == LOW) {
    alarmeMinute = (alarmeMinute + 5) % 60;
    tempsAffichageAlarme = millis();
    alarmeArmee = true;
    alarmeDeclenchee = false;
    delay(200);
    return;
  }
}

void verifierDeclenchementAlarme() {
  if (currentHour == alarmeHeure && currentMinute == alarmeMinute && currentSecond == 0 && alarmeArmee && !alarmeDeclenchee) {
    alarmeEnCours = true;
    alarmeDeclenchee = true;
    debutAlarme = millis();
    // réinitialiser notification pour ce nouveau cycle d'alarme
    notificationSent = false;
  }
  if (currentMinute != alarmeMinute || currentHour != alarmeHeure) {
    alarmeDeclenchee = false;
  }
}

void gererBuzzerEtLED() {
  if (alarmeEnCours) {
    unsigned long tempsEcoule = millis() - debutAlarme;
    if (tempsEcoule >= DUREE_SONNERIE_MAX) {
      alarmeEnCours = false;
      noTone(BUZZER_PIN);
      if (!notificationSent) {
        envoyerNotification();
      }
    }

    unsigned long DUREE_SON = 5 * 60 * 1000;
    unsigned long DUREE_PAUSE = 5 * 60 * 1000;
    unsigned long CYCLE_TOTAL = DUREE_SON + DUREE_PAUSE;
    unsigned long tempsDansLeCycle = tempsEcoule % CYCLE_TOTAL;

    if (tempsDansLeCycle < DUREE_SON) {
      unsigned long cycle = (millis() / 250) % 4;
      if (cycle == 0) {
        tone(BUZZER_PIN, 800);
        digitalWrite(LED_ROUGE, HIGH);
      } else if (cycle == 1) {
        noTone(BUZZER_PIN);
        digitalWrite(LED_ROUGE, LOW);
      } else if (cycle == 2) {
        tone(BUZZER_PIN, 1200);
        digitalWrite(LED_ROUGE, HIGH);
      } else {
        noTone(BUZZER_PIN);
        digitalWrite(LED_ROUGE, LOW);
      }
    } else {
      noTone(BUZZER_PIN);
      digitalWrite(LED_ROUGE, HIGH);
    }
  } else {
    noTone(BUZZER_PIN);
    digitalWrite(LED_ROUGE, HIGH);
  }
}

void gererAffichage() {
  if (millis() - tempsAffichageAlarme < 2000) {
    int timeAlarme = (alarmeHeure * 100) + alarmeMinute;
    display.showNumberDecEx(timeAlarme, 0b11100000, true);
  } else if (currentSecond >= 50 && currentSecond <= 55) {
    int dateFormatee = (currentDay * 100) + currentMonth;
    display.showNumberDecEx(dateFormatee, 0b01000000, true);
  } else {
    int displayTime = (currentHour * 100) + currentMinute;
    uint8_t dots = (currentSecond % 2 == 0) ? 0b11100000 : 0;
    display.showNumberDecEx(displayTime, dots, true);
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("=== DEMARRAGE DU REVEIL ESP8266 ===");

  initHardware();
  connectWiFi();
}

void loop() {
  lireHeureRTC();

  // Resynchronisation automatique en arrière-plan toutes les 6 heures
  if (millis() - dernierMiseAJourNTP > INTERVALLE_NTP && WiFi.status() == WL_CONNECTED) {
    synchroniserNTP();
  }

  gererBoutons();
  verifierDeclenchementAlarme();
  gererBuzzerEtLED();
  gererAffichage();

  delay(50);
}
