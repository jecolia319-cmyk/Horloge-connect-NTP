# Horloge_Alarme

Petit réveil basé sur ESP8266, RTC DS1302 et afficheur TM1637.

## Fonctionnalités
- Affichage heure/date sur TM1637
- Synchronisation NTP et écriture dans le RTC DS1302
- Réglage alarme (boutons Heure / Minute)
- Arrêt alarme (bouton STOP)
- Buzzer + LED pendant la sonnerie
- Envoi d'une notification HTTP si l'alarme n'est pas arrêtée (webhook IoT)

## Fichiers clés
- `src/main.cpp` : logique principale (initialisation, loop, alarmes, notifications)

## Configuration
- Mettre `ssid` et `password` dans `src/main.cpp` (ou extraire dans un fichier `config.h` ignoré par git).
- Définir `APP_ID`, `APP_KEY` et `server` dans `src/main.cpp` si vous voulez utiliser les notifications.

Important : évitez de commiter vos identifiants Wi‑Fi et clés API dans le dépôt public.

## Matériel
- ESP8266 (NodeMCU / Wemos)
- Module RTC DS1302 (connecté aux broches définies dans `src/main.cpp`)
- Afficheur 4 digits TM1637 (pins `DISPLAY_CLK`, `DISPLAY_DIO`)
- Buzzer + LED
- 3 boutons (Heure, Minute, Stop)

## Brochage (par défaut dans le code)
- `DISPLAY_CLK` = D1
- `DISPLAY_DIO` = D2
- `BTN_HEURE` = D3
- `BTN_MINUTE` = D4
- `BTN_STOP` = GPIO3 (RX)
- `RTC_RST` = D7, `RTC_CLK` = D5, `RTC_DAT` = D6
- `BUZZER_PIN` = D0, `LED_ROUGE` = D8

## Compilation et téléversement (PlatformIO)
Ouvrir le projet dans VS Code + PlatformIO puis :

```bash
# Compiler
platformio run

# Téléverser
platformio run --target upload
```

## Notes et améliorations possibles
- Remplacer `delay()` par debounce non bloquant pour une meilleure réactivité
- Stocker les réglages d'alarme dans la RAM du RTC ou SPIFFS pour persister après coupure
- Extraire les identifiants et clés dans un fichier `config.h` ignoré par `.gitignore`
- Ajouter gestion des erreurs réseau et retries pour les notifications

## Dépannage
- Si la connexion Wi‑Fi échoue, vérifiez SSID/mot de passe et la portée.
- Si l'heure NTP ne se synchronise pas, vérifiez que l'appareil a internet et que `configTime()` est supporté.

---

Si tu veux, je peux :
- ajouter un `config.h.example` pour centraliser les identifiants,
- implémenter un debounce non bloquant,
- ou lancer une compilation PlatformIO et corriger d'éventuelles erreurs.
