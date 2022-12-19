#if !defined(_CONTROL_H_)
    #define _CONTROL_H_

struct sensor_info_t {
    float resistencia = 0;
    float temperatura = 0;
};

struct control_info_t {
    bool automatic_control = false;

    float setpoint = 0;
    float tolerancia = 10;
    
    bool out_relay1 = false;
};

sensor_info_t sensor_info;
control_info_t control_info;

void update_control_info() {
    // TODO: codigo de controle
    // Calcular variaveis de saida ex: out_relay1
    // a partir dos dados de sensor_info
}

#endif // _CONTROL_H_
