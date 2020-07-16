#include "HMC5883.h"

#define M_PI 3.1415926

void Mag_Init(I2C_HandleTypeDef *hi2c){
	Mag_hi2c = *hi2c;
	SetDeclination(23, 35, 'E');
	SetSamplingMode(COMPASS_SINGLE);
	SetScale(COMPASS_SCALE_130);
	SetOrientation(COMPASS_HORIZONTAL_X_NORTH);
}

void SetScale(uint16_t scale){
	mode = (mode & ~0x1C) | (scale & 0x1C);
  Write(COMPASS_CONFIG_REGISTER_B, (( mode >> 2 ) & 0x07) << 5);
}

void SetOrientation(uint16_t orientation){
	mode = (mode & ~0x3FE0) | (orientation & 0x3FE0);
}

void SetDeclination(int declination_degs, int declination_mins, char declination_dir){
	switch(declination_dir){
    // North and East are positive   
    case 'E': 
      declination_offset_radians = ( declination_degs + (1/60 * declination_mins)) * (M_PI / 180);
      break;
    // South and West are negative    
    case 'W':
      declination_offset_radians = -(( declination_degs + (1/60 * declination_mins) ) * (M_PI / 180));
      break;
  }
}

void SetSamplingMode(uint16_t sampling_mode){
	mode = (mode & ~0x03) | (sampling_mode & 0x03);
  Write(COMPASS_MODE_REGISTER, mode & 0x03); 
}

float GetHeadingDegrees(){
	MagnetometerSample sample = ReadAxes();
  float heading;    
  // Determine which of the Axes to use for North and West (when compass is "pointing" north)
  float mag_north, mag_west;
  // Z = bits 0-2
  switch((mode >> 5) & 0x07 ){
    case COMPASS_NORTH: 
			mag_north = sample.Z; break;
    case COMPASS_SOUTH: 
			mag_north = -sample.Z; break;
    case COMPASS_WEST:  
			mag_west  = sample.Z; break;
    case COMPASS_EAST:  
			mag_west  = -sample.Z; break;
    // Don't care
    case COMPASS_UP:
    case COMPASS_DOWN:
			break;
  }
  // Y = bits 3 - 5
  switch(((mode >> 5) >> 3) & 0x07 ){
    case COMPASS_NORTH: 
			mag_north = sample.Y;  break;
    case COMPASS_SOUTH: 
			mag_north = -sample.Y; ;  break;
    case COMPASS_WEST:  
			mag_west  = sample.Y;  break;
    case COMPASS_EAST:  
			mag_west  = -sample.Y;  break;
    // Don't care
    case COMPASS_UP:
    case COMPASS_DOWN:
			break;
  }
  // X = bits 6 - 8
  switch(((mode >> 5) >> 6) & 0x07 ){
    case COMPASS_NORTH: 
			mag_north = sample.X; break;
    case COMPASS_SOUTH: 
			mag_north = -sample.X; break;
    case COMPASS_WEST:  
			mag_west  = sample.X; break;
    case COMPASS_EAST:  
			mag_west  = 0-sample.X; break;
    // Don't care
    case COMPASS_UP:
    case COMPASS_DOWN:
			break;
  }
  // calculate heading from the north and west magnetic axes
  heading = atan2(mag_west, mag_north);
  // Adjust the heading by the declination
  heading += declination_offset_radians;
  // Correct for when signs are reversed.
  if(heading < 0)
    heading += 2 * M_PI;
  // Check for wrap due to addition of declination.
  if(heading > 2 * M_PI)
    heading -= 2 * M_PI;
  // Convert radians to degrees for readability.
  return heading * 180 / M_PI; 
}

void Write(uint8_t address, char byte){
	uint8_t data_t[2];
	data_t[0] = byte;
	HAL_I2C_Mem_Write(&Mag_hi2c, COMPASS_I2C_ADDRESS, address, 1, (uint8_t*) data_t, 1, 1000);
}

uint8_t Read(uint8_t register_address, char buffer[], uint8_t length){
	HAL_I2C_Mem_Read(&Mag_hi2c, COMPASS_I2C_ADDRESS, register_address, 1, (uint8_t*) buffer, length, 1000);
	return 0;
}

MagnetometerSample ReadAxes(){
	if(mode & COMPASS_SINGLE){    
    Write(COMPASS_MODE_REGISTER, (uint8_t)( mode & 0x03 ));  
    HAL_Delay(60); 
  }
  
  char buffer[6];
  Read(COMPASS_DATA_REGISTER, buffer, 6);

  MagnetometerSample sample;
	
  // NOTE:
  // The registers are in the order X Z Y  (page 11 of datasheet)
  // the datasheet when it describes the registers details then in order X Y Z (page 15)
  // stupid datasheet writers
  sample.X = (buffer[0] << 8) | buffer[1];  
	if(sample.X > 32768) sample.X -= 65536;
  sample.Z = (buffer[2] << 8) | buffer[3];
	if(sample.Z > 32768) sample.Z -= 65536;
  sample.Y = (buffer[4] << 8) | buffer[5];
	if(sample.Y > 32768) sample.Y -= 65536;
  
  return sample;
}
