// Init Commands
#define AT_CPIN "AT+CPIN?\r"
#define ATE "ATE0\r"

// MQTT Commands
#define AT_MQTTSTART "AT+CMQTTSTART\r"
#define AT_MQTTACCQ "AT+CMQTTACCQ=%d, \"%.*s\"\r"
#define AT_MQTTWILLTOPIC                                                       \
    "AT+CMQTTWILLTOPIC=%d,%d\r" // Client index, length of topic
                                // Topic name on next line
#define AT_MQTTWILLMSG                                                         \
    "AT+CMQTTWILLMSG=%d,%d,%d\r" // Client index, length of topic, qos
                                 // Will messgae on next line

#define AT_MQTTCONNECT                                                         \
    "AT+CMQTTCONNECT=%d, \"%.*s\", %d, %d\r" // client index, server address,
                                             // keepalive, clean session
// Probably need username/password on
// the command or mTLS

#define AT_MQTTSUB                                                             \
    "AT+CMQTTSUB=%d,%d,%d\r" // Client index, length of topic, qos
                             // Topic to be subscribed on next line
#define AT_MQTTTOPIC "AT+CMQTTTOPIC=%d,%d\r" // Client index, length of topic
// Topic to be published on next line

#define AT_MQTTPAYLOAD                                                         \
    "AT+CMQTTPAYLOAD=%d,%d\r" // Client index, length of topic
                              // Topic to be published on next line

#define AT_MQTTPUB "AT+CMQTTPUB=%d,%d,%d\r" // Client index, qos, timeout

// Stop Commands
#define AT_MQTTDIS "AT+CMQTTDISC=%d,%d\r" // Client index, timeout
#define AT_MQTTREL "AT+CMQTTREL=%d\r"     // Client indes
#define AT_MQTTSTOP "AT+CMQTTSTOP\r"
