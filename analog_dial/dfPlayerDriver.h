#include "pico/stdlib.h"

#include "dfPlayer/dfPlayer.h"

template<uint8_t UART, uint8_t TX_PIN, uint8_t RX_PIN>
class DfPlayerPico : public DfPlayer<DfPlayerPico<UART, TX_PIN, RX_PIN>>
{
    public:
    DfPlayerPico();
    inline void uartSend(uint8_t* a_cmd);
    
    private:

};

template<uint8_t UART, uint8_t TX_PIN, uint8_t RX_PIN>
DfPlayerPico<UART, TX_PIN, RX_PIN>::DfPlayerPico()
{
    if constexpr (UART == 0)
      uart_init(uart0, 9600);
    else
      uart_init(uart1, 9600);
 
    // Set the GPIO pin mux to the UART - 8 is TX, 9 is RX
    gpio_set_function(TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(RX_PIN, GPIO_FUNC_UART);
}

template<uint8_t UART, uint8_t TX_PIN, uint8_t RX_PIN>
inline void DfPlayerPico<UART, TX_PIN, RX_PIN>::uartSend(uint8_t* a_cmd)
{
    if constexpr (UART == 0)
      uart_write_blocking(uart0, a_cmd, SERIAL_CMD_SIZE);
    else 
      uart_write_blocking(uart1, a_cmd, SERIAL_CMD_SIZE);
}
