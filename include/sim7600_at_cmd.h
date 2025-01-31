// Init Commands
#define AT_CPIN "AT+CPIN?\r"
#define ATE "ATE0\r"

// MQTT Commands
#define AT_MQTTSTART "AT+MQTTSTART\r"
#define AT_MQTTACCQ "AT+CMQTTACCQ=%d, \"%.*s\"\r"
#define AT_MQTTWILLTOPIC                                                       \
    "AT+CMQTTWILLTOPIC=%d,%d\r" // Client index, length of topic
                                // Topic name on next line
#define AT_MQTTWILLMSG                                                         \
    "AT+CMQTTWILLMSG=%d,%d,%d\r" // Client index, length of topic, qos
                                 // Will messgae on next line

#define AT_MQTTCONNECT                                                         \
    "AT+CMQTTCONNECT=%d, \"%.*s\", %d, %d" // client index, server address,
                                           // keepalive, clean session
                                           // Probably need username/password on
                                           // the command or mTLS

#define AT_MQTTSUB                                                             \
    "AT+CMQTTSUB=%d,%d,%d" // Client index, length of topic, qos
                           // Topic to be subscribed on next line
#define AT_MQTTTOPIC                                                           \
    "AT+CMQTTTOPIC=%d,%d" // Client index, length of topic
                          // Topic to be published on next line

#define AT_MQTTPAYLOAD                                                         \
    "AT+CMQTTPAYLOAD=%d,%d" // Client index, length of topic
                            // Topic to be published on next line

#define AT_MQTTPUB "AT+CMQTTPUB=%d,%d,%d" // Client index, qos, timeout
