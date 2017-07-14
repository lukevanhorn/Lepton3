#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <limits.h>
#include "libs/raspi_I2C.h"

#include "libs/LEPTON_SDK.h"
#include "libs/LEPTON_SYS.h"
#include "libs/LEPTON_Types.h"
#include "libs/LEPTON_ErrorCodes.h"
#include "libs/LEPTON_I2C_Protocol.h"
#include "libs/LEPTON_I2C_Service.h"

LEP_CAMERA_PORT_DESC_T _port;
LEP_UINT16 port = 1;
LEP_UINT16 statusReg;
LEP_UINT16 _status;

//extern LEP_RESULT LEP_I2C_OpenPort(LEP_UINT16 portID,
//                                       LEP_UINT16 *baudRateInkHz,
//                                       LEP_UINT8 *deviceAddress);


int main(int argc, char *argv[])
{
  LEP_RESULT _result = 0;
  LEP_UINT16 _read = 0;

  _result = DEV_I2C_MasterInit(port, (LEP_UINT16 *)LEP_TWI_CLOCK_400KHZ);
  //_result = LEP_I2C_OpenPort(port, (LEP_UINT16 *)LEP_TWI_CLOCK_400KHZ, LEP_CCI_TWI);

  printf("\nopen: %d\n", _result);

  _result = DEV_I2C_MasterReadData(port,
                                0x00,
                                0x0002,
                                &statusReg,
                                1, &_read, &_status  );

  printf("\n booted: %d boot mode: %d busy: %d read: %d status: %d\n", (statusReg & 0x4) >> 2, (statusReg & 0x2) >> 1, (statusReg & 0x1), _read, _status);

  _result = DEV_I2C_MasterClose();

  printf("\n close: %d\n", _result);

  //_result = LEP_I2C_GetPortStatus(&_port);

  //printf("\n get port status returned %d\n", _result);

  return _result;
}

