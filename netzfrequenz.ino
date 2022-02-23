
//  input capture = pin8
const uint8_t input_capture_pin = 8;

const uint8_t magic_calibration = 100;

// helper type for efficient access of the counter structure 
typedef union {
    uint32_t clock_ticks;
    struct {
        uint8_t  byte_0;
        uint8_t  byte_1;
        uint16_t word_1;
    };
} converter;

// The timer values will be initialized nowhere.
// This works because we are intested in the differences only.
// The first differences will be meaningless due to lack
// of initialization. However they would be meaningless anyway
// because the very first capture has (by definition) no predecessor.
// So the lack of initialization semantically reflects this.
volatile converter input_capture_time;
volatile uint16_t timer1_overflow_count;
 
// 0 indicates "invalid"
volatile uint32_t period_length = 0;
volatile bool next_sample_ready = false;
 
ISR(TIMER1_OVF_vect) {
    ++timer1_overflow_count;
}
 
ISR(TIMER1_CAPT_vect) { 
    static uint32_t previous_capture_time = 0;
   
    // according to the datasheet the low byte must be read first
    input_capture_time.byte_0 = ICR1L;
    input_capture_time.byte_1 = ICR1H;

    if ((TIFR1 & (1<<TOV1) && input_capture_time.byte_1 < 128)) {
        // we have a timer1 overflow AND
        // input capture time is so low that we must assume that it
        // was wrapped around
        ++timer1_overflow_count;
        // we clear the overflow bit in order to not trigger the
        // overflow ISR, otherwise this overflow would be
        // counted twice
        TIFR1 = (1<<TOV1);
    }
    input_capture_time.word_1 = timer1_overflow_count;

    period_length = input_capture_time.clock_ticks - previous_capture_time;
    previous_capture_time = input_capture_time.clock_ticks;

    next_sample_ready = true;
}                 
                  
void initialize_timer1() {
    // Timer1: "normal mode", no automatic toggle of output pins
    //                        wave form generation mode  with Top = 0xffff
    TCCR1A = 0;                
    // Timer 1: input capture noise canceler active
    //          input capture trigger on rising edge
    //          clock select: no prescaler, use system clock 
    TCCR1B = (1<<ICNC1) | (1<< ICES1) | (1<<CS10);
    // Timer1: enable input capture and overflow interrupts
    TIMSK1 = (1<<ICIE1) | (1<<TOIE1);
    // Timer1: clear input capture and overflow flags by writing a 1 into them    
    TIFR1 = (1<<ICF1) | (1<<TOV1) ;            
}    

void setup() {
    Serial.begin(115200);
    Serial.print(F("STARTUP"));

    delay(1000);

    pinMode(input_capture_pin, INPUT);
    digitalWrite(input_capture_pin, HIGH);

    initialize_timer1();
}

const uint8_t sample_buffer_size = 50;

uint32_t sample_buffer[sample_buffer_size];

void loop() {
    static uint8_t sample_index = 0;

    if (next_sample_ready) {
        next_sample_ready = false;

        cli();
        sample_buffer[sample_index] = period_length;
        sei();
        sample_index = sample_index > 0 ? sample_index - 1 : sample_buffer_size - 1;
     
        uint32_t average_period_length = 0;
        for (uint8_t index = 0; index < sample_buffer_size; ++index) {
            average_period_length += sample_buffer[index];
        }
        average_period_length /= sample_buffer_size;        

        const int64_t deviation_1000 = 1000*(int64_t)F_CPU / average_period_length;
        Serial.println((int32_t)(deviation_1000 + magic_calibration) / 2);
    }
}


