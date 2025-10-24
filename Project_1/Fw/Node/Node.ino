#include <SPI.h>
#include <LoRa.h>

// ================== LoRa PINS =====================
#define ss    PB0
#define rst   PA9
#define dio0  PA3

// ================== IO PINS =======================
#define LED        PB8
#define PWM_PIN    PA4   // DAC output
#define RELAY_PIN  PB7

// ================== Calibration ===================
#define TEMP110_CAL_ADDR ((unsigned short *)((unsigned int)0x1FFFF7C2))
#define TEMP30_CAL_ADDR  ((unsigned short *)((unsigned int)0x1FFFF7B8))
#define VDD_APPLI (3000.0)  // 3.0V in mV

// ================== Structs =======================
// Master gửi xuống
struct LoRaPacket {
  int id;       // Node ID
  bool data1;   // Relay ON/OFF
  int data2;    // PWM 0-255
};

// Node gửi lên
struct LoRaPacketSend {
  int id;
  float data1;        // temperature
  unsigned long data2; // uptime (s)
};

// ================== Globals =======================
LoRaPacket receivedPacket;
LoRaPacketSend packetToSend;

int idSlave = 3;               // Node ID (thay đổi cho mỗi node)
float temperature;
volatile unsigned long systemUptime = 0;
unsigned long lastMillis = 0;

// ================== Setup =========================
void setup() {
  // Init IO
  pinMode(LED, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);

  // Init DAC (PA4)
  DAC_Init();
  DAC_SetPercentage(0); // mặc định 0%

  digitalWrite(RELAY_PIN, LOW);

  // Init ADC để đọc temp sensor
  ADC_Init();

  // Init LoRa
  LoRa.setPins(ss, rst, dio0);
  if (!LoRa.begin(433E6)) {
    while (1); // lỗi thì treo luôn
  }
  LoRa.setSyncWord(0xF3);
}

// ================== Loop ==========================
void loop() {
  // Đếm uptime
  if (millis() - lastMillis >= 1000) {
    lastMillis = millis();
    systemUptime++;
  }

  // Nhận lệnh từ Master
  int packetSize = LoRa.parsePacket();
  if (packetSize == sizeof(LoRaPacket)) {
    LoRa.readBytes((uint8_t *)&receivedPacket, sizeof(receivedPacket));

    if (receivedPacket.id == idSlave) {
      // Điều khiển relay
      digitalWrite(RELAY_PIN, receivedPacket.data1);
      digitalWrite(LED, receivedPacket.data1 ? HIGH : LOW);

      // Điều khiển dimming (0-255)
      uint8_t pwmPercent = map(receivedPacket.data2, 0, 255, 0, 100);
      DAC_SetPercentage(pwmPercent);

      // Đọc nhiệt độ chip
      temperature = readChipTemperature();

      // Gửi phản hồi về Master
      packetToSend.id = idSlave;
      packetToSend.data1 = temperature;
      packetToSend.data2 = systemUptime;

      LoRa.beginPacket();
      LoRa.write((uint8_t *)&packetToSend, sizeof(packetToSend));
      LoRa.endPacket();
    }
  }
}

// ================== Functions =====================
float readChipTemperature() {
  unsigned short adc_value = ADC_Read();
  float v_sense = (adc_value * VDD_APPLI) / 4095.0;
  float temp30 = (float)(*TEMP30_CAL_ADDR);
  float temp110 = (float)(*TEMP110_CAL_ADDR);
  return ((v_sense - temp30) * (110.0 - 30.0)) / (temp110 - temp30) + 30.0;
}

void ADC_Init(void) {
  RCC->APB2ENR |= RCC_APB2ENR_ADCEN;
  ADC1->CHSELR = ADC_CHSELR_CHSEL16;
  ADC1->SMPR |= ADC_SMPR_SMP_0 | ADC_SMPR_SMP_1 | ADC_SMPR_SMP_2;
  ADC->CCR |= ADC_CCR_TSEN;
  ADC1->CR |= ADC_CR_ADEN;
  while (!(ADC1->ISR & ADC_ISR_ADRDY));
}

int ADC_Read(void) {
  ADC1->CR |= ADC_CR_ADSTART;
  while (!(ADC1->ISR & ADC_ISR_EOC));
  return ADC1->DR;
}

void DAC_Init(void) {
  RCC->AHBENR |= RCC_AHBENR_GPIOAEN;
  RCC->APB1ENR |= RCC_APB1ENR_DACEN;
  GPIOA->MODER |= (3U << (4 * 2)); // PA4 analog mode
  DAC->CR |= DAC_CR_EN1; // enable DAC ch1
}

// Set DAC output (0–100%)
void DAC_SetPercentage(uint8_t percentage) {
  if (percentage > 100) percentage = 100;
  uint16_t dac_value = (percentage * 4095) / 100;
  DAC->DHR12R1 = dac_value;
}
