MorangoBot - Sistema Embarcado de Diagnóstico Multiespectral

- Concurso Agrinho 2026 | Categoria AgroRobótica
- Tema: *Agro forte, futuro sustentável: equilíbrio entre produção e meio ambiente*

:: Sobre o Projeto ::

- O MorangoBot é um sistema IoT embarcado de baixo custo desenvolvido para auxiliar produtores rurais no diagnóstico precoce de estresses nutricionais, hídricos e fitossanitários em cultivos de morango em ambiente protegido.

- Desenvolvido pela **EQUIPE: EMANUELLA / KAUANE / LAURA / PAOLA / EMANUELA** do CEEP Olegário Macedo (Castro/PR), o projeto atende ao desafio proposto pelo Consultor Agrícola Robert Correa Anhaia e o parceiro Luiz Antonio Lopes, produtor da cidade de Piraí do Sul/Pr, promovendo a agricultura de precisão acessível e reduzindo o uso desnecessário de insumos químicos e água.

- Máquina de Estados Finita (FSM) Não-Bloqueante:** Substituição total de funções `delay()` por verificação temporal via `millis()` com 4 estados discretos (`IDLE`, `LED_ON`, `READ_TCS`, `FINALIZE`). Isso permite que o servidor HTTP continue respondendo a requisições assíncronas durante os 3 segundos de estabilização do sensor espectral.

- Algoritmos Agronômicos Otimizados: Implementação nativa das fórmulas de Tetens (VPD) e Magnus-Tetens (Ponto de Orvalho) otimizadas para aritmética de ponto flutuante em microcontroladores, evitando dependência de bibliotecas pesadas.

- Matriz de Decisão Multivariada: Lógica de inferência com 7 regras encadeadas por severidade que cruza dados edáficos, espectrais e microclimáticos, eliminando falsos positivos comuns em análises univariadas.

- Rotina de Anti-Travamento I2C: Implementação de recuperação de barramento com 9 pulsos de clock manuais no `setup()`, baseada na errata técnica do ESP32-S3, garantindo estabilidade operacional pós-reset ou brownout.

- Servidor HTTP com UTF-8 Forçado: Cabeçalhos `charset=utf-8` explícitos em todas as rotas RESTful e meta-tag HTML correspondente, resolvendo definitivamente problemas de codificação de caracteres especiais (emojis) em navegadores móveis.

- Código Limpo e Parametrizado: Declaração centralizada de todos os pinos, thresholds e constantes via `#define` e `const`. Zero valores literais ("magic numbers") no corpo das funções, facilitando calibração e manutenção.

Diferenciais Técnicos
- Edge Computing: Processamento 100% local no ESP32-S3 (funciona sem internet)
- Dual I2C Bus: Arquitetura com barramentos independentes (`Wire` e `Wire1`) para estabilidade multi-sensor
- Máquina de Estados Finita (FSM): Leitura não-bloqueante que mantém o servidor web responsivo
- Algoritmos Agronômicos: Cálculo em tempo real de VPD, Ponto de Orvalho e NDVI Proxy
- Interface Web Offline: Servidor HTTP embutido com design responsivo e UTF-8

Hardware Utilizado
| Componente | Modelo | Função | Pino (ESP32-S3)

- Microcontrolador: ESP32-S3 N16R8
- Sensor Ambiental: GY-BME280 >> Temperatura Umidade, Pressão do ar >> SDA: GPIO1, SCL: GPIO2 (Wire1)
- Sensor Espectral TCS34725: Clorofila / NDVI Proxy                 >> SDA: GPIO8, SCL: GPIO9 (Wire)
- Sensor de Solo capacitivo S12: Umidade do solo                    >> AOUT: GPIO3
- LED do TCS: Integrado, Iluminação controlada                      >> GPIO10
- Push Button: Acionamento diagnóstico                              >> GPIO4

### ⚡ Diagrama de Ligação
ESP32-S3 N16R8
├── GPIO1 (SDA) ─── BME280 (Wire1)
├── GPIO2 (SCL) ─── BME280 (Wire1)
├── GPIO8 (SDA) ─── TCS34725 (Wire)
├── GPIO9 (SCL) ─── TCS34725 (Wire)
├── GPIO3 (ADC) ─── Sensor Solo S12 (AOUT)
├── GPIO4 ───────── Push Button → GND
├── GPIO10 ──────── TCS LED Pin
├── 3.3V ────────── VCC (BME, TCS, Solo)
└── GND ─────────── GND (Todos)
