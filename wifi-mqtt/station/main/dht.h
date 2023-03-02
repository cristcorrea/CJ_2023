#ifndef DHT_H
#define DHT_H


typedef struct 
{
    float temperature;
    int humidity; 
    
}dht;

dht dht_data;  

void DHTerrorHandler(int response); 

int readDHT();

#endif