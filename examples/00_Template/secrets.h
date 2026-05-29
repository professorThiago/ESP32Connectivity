//! secrets.h
//! Copie este arquivo para src/secrets.h no seu projeto.
//! Não edite este arquivo — edite a cópia no seu projeto.

#ifndef SECRETS_H
#define SECRETS_H

// =============================
// WiFi
// =============================

extern const char* WIFI_SSID;
extern const char* WIFI_SENHA;

// =============================
// MQTT
// =============================

extern const char* MQTT_BROKER;
extern const int   MQTT_PORTA;
extern const char* MQTT_CLIENT_ID;
extern const char* MQTT_USUARIO;
extern const char* MQTT_SENHA;
extern const bool  MQTT_TLS;
extern const char  MQTT_CERTIFICADO_CA[];

// =============================
// AWS IoT Core
// =============================

extern const bool  USAR_AWS_IOT;
extern const char* AWS_IOT_ENDPOINT;
extern const int   AWS_IOT_PORT;
extern const char* AWS_IOT_CLIENT_ID;
extern const char  AWS_CERT_CA[];
extern const char  AWS_CERT_CRT[];
extern const char  AWS_CERT_PRIVATE[];

// =============================
// Tópicos MQTT
// =============================

extern const char* TOPICOS_PUBLICAR[];
extern const int   TOTAL_TOPICOS_PUBLICAR;

extern const char* TOPICOS_RECEBER[];
extern const int   TOTAL_TOPICOS_RECEBER;

// =============================
// Debug
// =============================

extern const int DEBUG_NIVEL_INICIAL;
extern const int PINO_HABILITA_DEBUG_COMPLETO;

#endif
