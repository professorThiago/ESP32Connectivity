# ConectividadeESP32

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

#### Como funciona

Toda mensagem passa pela fila **antes** de ser enviada — mesmo quando o MQTT está conectado. Isso garante que nenhuma mensagem seja perdida em caso de queda de conexão entre a chamada a `publicar()` e o envio efetivo pelo TCP:

```
publicar("topico", "payload")
        │
        ├── 1. Enfileira a mensagem (sempre)
        │
        └── 2. MQTT conectado?
                  ├── Sim → tenta drenar a fila agora
                  │           ├── Envio ok  → remove da fila → retorna true
                  │           └── Falhou   → fica na fila  → retorna false
                  └── Não → fica na fila até reconectar   → retorna false
```

A fila é drenada automaticamente pelo `update()` a cada `loop()` quando a conexão é restabelecida.

```cpp
// Publica — sempre enfileira antes de enviar
conectividade.publicar(0, "payload");          // por índice
conectividade.publicar("topico/x", "payload"); // por nome

// Consulta quantas mensagens estão aguardando
conectividade.mensagensNaFila();   // uint8_t
```

#### Configurando o tamanho da fila

O tamanho da fila é definido por `#define` em tempo de compilação. Declare os defines **antes** do `#include <ESP32Connectivity.h>` no `main.cpp`:

```cpp
// ⚠ DEVE vir ANTES do #include
#define CONNECTIVITY_FILA_SLOTS       20   // número de mensagens (padrão: 10)
#define CONNECTIVITY_FILA_TOPICO_MAX 128   // bytes por tópico  (padrão: 64)
#define CONNECTIVITY_FILA_PAYLOAD_MAX 512  // bytes por payload (padrão: 256)

#include <ESP32Connectivity.h>
```

> Não existe método runtime para alterar o tamanho — a fila é um array estático alocado em tempo de compilação, o que evita fragmentação de memória em embarcados.

| Define | Padrão | Descrição |
|--------|--------|-----------|
| `CONNECTIVITY_FILA_SLOTS` | `10` | Número máximo de mensagens na fila |
| `CONNECTIVITY_FILA_TOPICO_MAX` | `64` | Tamanho máximo do tópico em bytes |
| `CONNECTIVITY_FILA_PAYLOAD_MAX` | `256` | Tamanho máximo do payload em bytes |

Quando a fila está cheia e uma nova mensagem chega, a **mensagem mais antiga é descartada** e um aviso é emitido no log (`[AVISO]`).

#### Buffer do PubSubClient (tamanho de cada pacote MQTT)

Diferente da fila, o buffer do PubSubClient controla o tamanho máximo de **cada mensagem individualmente**. O padrão é 256 bytes — se seu payload for maior, aumente antes do `begin`:

```cpp
conectividade.configurarBufferMQTT(1024);   // mensagens até 1 KB
conectividade.beginSimples(wifi, mqtt, topicos);
```

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

---

## ESP32-S3 N16R8 — usando PSRAM para a fila

O modelo N16R8 tem 8 MB de PSRAM externa. Por padrão a fila é alocada na SRAM interna (512 KB). Com PSRAM habilitada, a fila pode ter centenas de slots sem impactar o restante do programa.

### platformio.ini para N16R8 com PSRAM

```ini
[env:esp32-s3-n16r8]
platform  = espressif32
board     = esp32-s3-devkitm-1
framework = arduino

board_build.arduino.memory_type = qio_opi
board_upload.flash_size          = 16MB
board_build.partitions           = default_16MB.csv

build_flags =
    -DBOARD_HAS_PSRAM
    -DCONNECTIVITY_USAR_PSRAM=1      ; aloca a fila na PSRAM
    -DCONNECTIVITY_FILA_SLOTS=200    ; 200 slots × 324 bytes = ~63 KB na PSRAM
```

### Quanto usar?

| Slots | RAM usada (slot padrão 64+256) | Indicado para |
|-------|-------------------------------|---------------|
| 20 | 6,3 KB — SRAM | Uso geral sem PSRAM |
| 50 | 15,8 KB — SRAM | Logging moderado sem PSRAM |
| 200 | 63 KB — **PSRAM** | Telemetria, quedas longas |
| 500 | 158 KB — **PSRAM** | Logging intenso |
| 1000 | 316 KB — **PSRAM** | Máximo prático |

> A PSRAM é ~10× mais lenta que a SRAM interna, mas para uma fila de mensagens (acesso esporádico) isso é imperceptível. A biblioteca detecta automaticamente se a PSRAM está disponível — se `psramFound()` retornar `false`, aloca na SRAM sem travar.
