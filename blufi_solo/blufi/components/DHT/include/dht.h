
typedef struct 
{
    float temperature;
    int humidity; 
    
}dht;


extern dht DHT_DATA;
 

void DHTerrorHandler(int response); 

int readDHT();
