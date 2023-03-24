typedef struct
{
    float conductivity;
    int humidity; 
} soil;

extern soil SOIL_DATA; 

void soilConfig(void); 

//void conductivity(void);
void humidity(void);