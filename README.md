# ESP32Connectivity

Biblioteca Arduino/ESP32 para gerenciamento **não-bloqueante** de WiFi e MQTT, com reconexão automática e fila de mensagens offline.

> Autor: [professorThiago](https://github.com/professorThiago)  
> Depende de: [DebugManager](https://github.com/professorThiago/DebugManager) — baixado automaticamente.

---

## Por que não-bloqueante?

A abordagem tradicional usa `while()` para aguardar conexão:

```cpp
// ❌ Bloqueante — trava o setup() por até 15 segundos
while (WiFi.status() != WL_CONNECTED) { delay(500); }
```

Esta biblioteca usa **máquinas de estado** processadas no `loop()`:

```cpp
// ✓ Não-bloqueante — setup() retorna imediatamente
conectividade.beginSimples(wifi, mqtt, topicos);

void loop() {
    conectividade.update();   // avança sem travar
    // sua lógica continua normalmente
}
```

---

## Funcionalidades

- ✅ WiFi e MQTT não-bloqueantes com reconexão automática.
- ✅ Três modos de conexão: **SIMPLES**, **TLS** e **AWS IoT Core**.
- ✅ **Fila offline:** mensagens publicadas sem MQTT são salvas e enviadas ao reconectar.
- ✅ Sem dependência de `secrets.h` — tudo configurado via structs no `begin()`.
- ✅ Diagnóstico detalhado: códigos WiFi e MQTT traduzidos com dicas de solução.
- ✅ Buffer MQTT configurável via `configurarBufferMQTT()`.

---

## Instalação

```ini
lib_deps =
    https://github.com/professorThiago/ESP32Connectivity
```

O PlatformIO baixa automaticamente **DebugManager** e **PubSubClient**.

---

## Modos de conexão

| Modo | Enum | Quando usar |
|------|------|-------------|
| Sem criptografia | `ModoConexao::SIMPLES` | Redes locais confiáveis, testes |
| TLS | `ModoConexao::TLS` | Brokers públicos (HiveMQ, Mosquitto Cloud, etc.) |
| AWS IoT Core | `ModoConexao::AWS_IOT` | AWS com certificado de dispositivo (mTLS) |

O modo é determinado pela sobrecarga do `begin()` chamada — não há flag booleana ou variável global.

---

## Quick start

### Modo SIMPLES

```cpp
#include <DebugManager.h>
#include <ESP32Connectivity.h>

ConfigWiFi wifi = { "MinhaRede", "minha_senha" };
ConfigMQTT mqtt = { "192.168.1.100", 1883, "esp32_id", "", "" };

const char* pub[] = { "casa/status" };
const char* rec[] = { "casa/comando" };
ConfigTopicos topicos = { pub, 1, rec, 1 };

void setup() {
    configurarDebug(DEBUG_INFO, 4);
    conectividade.beginSimples(wifi, mqtt, topicos);
}

void loop() {
    conectividade.update();
    conectividade.publicar(0, "online");
    delay(5000);
}
```

### Modo TLS

```cpp
const char meuCertCA[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
...certificado CA do broker...
-----END CERTIFICATE-----
)EOF";

ConfigMQTT mqtt = { "broker.hivemq.com", 8883, "esp32_id", "usuario", "senha" };
ConfigTLS  tls  = { meuCertCA };

conectividade.beginTLS(wifi, mqtt, tls, topicos);
```

### Modo AWS IoT Core

```cpp
const char awsCertCA[]      PROGMEM = R"EOF( ...AmazonRootCA1... )EOF";
const char awsCertCRT[]     PROGMEM = R"CRT( ...certificado do dispositivo... )CRT";
const char awsCertPrivate[] PROGMEM = R"KEY( ...chave privada... )KEY";

ConfigAWS aws = {
    "xxxx.iot.us-east-1.amazonaws.com",
    8883,
    "meu_thing_name",
    awsCertCA, awsCertCRT, awsCertPrivate
};

conectividade.beginAWS(wifi, aws, topicos);
```

---

## Structs de configuração

### `ConfigWiFi`
```cpp
ConfigWiFi wifi = {
    "SSID",   // nome da rede
    "SENHA"   // senha
};
```

### `ConfigMQTT`
```cpp
ConfigMQTT mqtt = {
    "broker.exemplo.com",  // endereço do broker
    8883,                  // porta (1883 sem TLS, 8883 com TLS)
    "esp32_meu_projeto",   // clientId — único por dispositivo
    "usuario",             // "" para conexão anônima
    "senha"                // "" para conexão anônima
};
```

### `ConfigTLS`
```cpp
ConfigTLS tls = {
    meuCertCA   // ponteiro para o certificado CA em PEM (PROGMEM)
};
```
Deixe `certificadoCA = ""` para usar `setInsecure()` — **apenas para testes**.

### `ConfigAWS`
```cpp
ConfigAWS aws = {
    "xxxx.iot.us-east-1.amazonaws.com",  // endpoint
    8883,                                 // porta
    "meu_thing_name",                    // clientId
    awsCertCA,                           // AmazonRootCA1.pem
    awsCertCRT,                          // certificado do dispositivo
    awsCertPrivate                       // chave privada
};
```

### `ConfigTopicos`
```cpp
const char* pub[] = { "projeto/status", "projeto/dados" };
const char* rec[] = { "projeto/comando" };

ConfigTopicos topicos = {
    pub, 2,   // array de publicação + quantidade
    rec, 1    // array de recebimento + quantidade
};
```

---

## API

### Inicialização

```cpp
// Modo SIMPLES
conectividade.beginSimples(wifi, mqtt, topicos);

// Modo TLS
conectividade.beginTLS(wifi, mqtt, tls, topicos);

// Modo AWS IoT
conectividade.beginAWS(wifi, aws, topicos);
```

### Loop

```cpp
conectividade.update();   // SEMPRE no loop() — nunca omita
```

### Buffer MQTT (opcional, antes do begin)

```cpp
// Aumenta o limite para payloads maiores que 256 bytes
conectividade.configurarBufferMQTT(1024);
conectividade.beginSimples(wifi, mqtt, topicos);
```

| Parâmetro | Padrão | Máximo recomendado |
|-----------|--------|--------------------|
| Buffer MQTT | 256 bytes | 4096 bytes no ESP32 |

### Fila offline

```cpp
// Publica — enfileira automaticamente se offline
conectividade.publicar(0, "payload");          // por índice
conectividade.publicar("topico/x", "payload"); // por nome

// Consulta
conectividade.mensagensNaFila();   // uint8_t
```

| Define | Padrão | Como sobrescrever |
|--------|--------|-------------------|
| `CONNECTIVITY_FILA_SLOTS` | 10 | `#define CONNECTIVITY_FILA_SLOTS 20` antes do `#include` |
| `CONNECTIVITY_FILA_TOPICO_MAX` | 64 bytes | `#define CONNECTIVITY_FILA_TOPICO_MAX 128` |
| `CONNECTIVITY_FILA_PAYLOAD_MAX` | 256 bytes | `#define CONNECTIVITY_FILA_PAYLOAD_MAX 512` |

### Estado

```cpp
conectividade.wifiConectado();    // bool
conectividade.mqttConectado();    // bool
conectividade.mensagensNaFila();  // uint8_t
```

### Callbacks

```cpp
conectividade.registrarCallbackWiFiConectado(   []() { ... } );
conectividade.registrarCallbackWiFiDesconectado([]() { ... } );
conectividade.registrarCallbackMQTTConectado(   []() { ... } );
conectividade.registrarCallbackMQTTDesconectado([]() { ... } );
conectividade.registrarCallbackMensagem(
    [](const char* topico, const String& msg) { ... }
);
```

### Debug

```cpp
// Inicializa o sistema de log (chame antes do begin)
configurarDebug(DEBUG_INFO, 4);   // nível INFO, override no GPIO 4
configurarDebug(DEBUG_TUDO, -1);  // nível TUDO, sem override
```

| Nível | Constante | O que exibe |
|-------|-----------|-------------|
| 0 | `DEBUG_NENHUM` | Nada |
| 1 | `DEBUG_ERRO` | Falhas críticas |
| 2 | `DEBUG_AVISO` | + Reconexões e timeouts |
| 3 | `DEBUG_INFO` | + Fluxo normal |
| 4 | `DEBUG_VERBOSE` | + Tentativas e estados internos |
| 5 | `DEBUG_TUDO` | + Payloads e códigos de erro brutos |

---

## Exemplos

| Exemplo | Descrição |
|---------|-----------|
| `00_Template` | **Ponto de partida** — todos os modos comentados, pronto para parametrizar |
| `01_WiFiBasico` | Conexão WiFi não-bloqueante com callbacks |
| `02_MQTTBasico` | WiFi + MQTT com publicação periódica |
| `03_FilaOffline` | Demonstra enfileiramento e drenagem automática |

---

## Máquinas de estado

```
WiFi:  DESCONECTADO ──begin()──> CONECTANDO ──OK──> CONECTADO
            ^                        |                   |
            └── retry 5s ────────────┘ timeout 15s   queda

MQTT:  DESCONECTADO ──WiFi OK──> CONECTANDO ──OK──> CONECTADO
            ^                        |                   |
            └── retry 5s ───── falha (máx 5x)       broker caiu
```

---

## Licença

MIT © 2026 [professorThiago](https://github.com/professorThiago)
