typedef struct
{
    int salinity;
    int humidity; 
} soil;

extern soil SOIL_DATA; 

void soilConfig(void); 

void humidity(void);
void salt(void);
