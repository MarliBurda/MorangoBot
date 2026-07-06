MorangoBot - Sistema Embarcado de Diagnóstico Multiespectral

> Concurso Agrinho 2026 | Categoria AgroRobótica
> Tema: *Agro forte, futuro sustentável: equilíbrio entre produção e meio ambiente*

Sobre o Projeto

O **MorangoBot** é um sistema IoT embarcado de baixo custo desenvolvido para auxiliar produtores rurais no diagnóstico precoce de estresses nutricionais, hídricos e fitossanitários em cultivos de morango (*Fragaria × ananassa*) em ambiente protegido.

Desenvolvido pela **EQUIPE: EMANUELLA / KAUANE / LAURA / PAOLA / EMANUELA** do CEEP Olegário Macedo (Castro/PR), o projeto atende ao desafio proposto pelo produtor rural parceiro **Luiz Antonio Lopes**, promovendo a agricultura de precisão acessível e reduzindo o uso desnecessário de insumos químicos e água.

Diferenciais Técnicos
- Edge Computing: Processamento 100% local no ESP32-S3 (funciona sem internet)
- Dual I2C Bus: Arquitetura com barramentos independentes (`Wire` e `Wire1`) para estabilidade multi-sensor
- Máquina de Estados Finita (FSM): Leitura não-bloqueante que mantém o servidor web responsivo
- Algoritmos Agronômicos: Cálculo em tempo real de VPD, Ponto de Orvalho e NDVI Proxy
- Interface Web Offline: Servidor HTTP embutido com design responsivo e UTF-8

Hardware Utilizado

| Componente | Modelo | Função | Pino (ESP32-S3) |

- Microcontrolador: ESP32-S3 N16R8
- Sensor Ambiental: GY-BME280 >> Temperatura Umidade, Pressão do ar >> SDA: GPIO1, SCL: GPIO2 (Wire1)
- Sensor Espectral TCS34725: Clorofila / NDVI Proxy                 >> SDA: GPIO8, SCL: GPIO9 (Wire)
- Sensor de Solo capacitivo S12: Umidade do solo                    >> AOUT: GPIO3
- LED do TCS: Integrado, Iluminação controlada                      >> GPIO10
- Push Button: Acionamento diagnóstico                              >> GPIO4

### ⚡ Diagrama de Ligação
