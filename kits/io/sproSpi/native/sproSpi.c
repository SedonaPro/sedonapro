//
// Copyright 2016 Andrey Skvortsov <andrej.skvortzov@gmail.com>
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "sproSpi.h"
#include "sproUtil.h"
#include <stdint.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>

#include "sedona.h"

Cell sproSpi_SpiBaseApi_open(SedonaVM* vm, Cell* params)
{
//	FTRACE();

	Cell ret_cell;
	char* spidev = params[0].aval;
	int  fd;

        fd = open(spidev, O_RDWR);	
	if (fd < 0) {
		ERROR("can't open device: %s\n", spidev ? spidev : "NULL");
	}

	ret_cell.ival = fd;
	return ret_cell;
}

Cell sproSpi_SpiBaseApi_close(SedonaVM* vm, Cell* params)
{
//	FTRACE();
	Cell ret_cell;
  	
	ret_cell.ival = close(params[0].ival);
	return ret_cell;
}

Cell sproSpi_SpiBaseApi_sendByte(SedonaVM* vm, Cell* params)
{
//	FTRACE();

	Cell             ret_cell;
	int fd         = params[0].ival;
	int freq_hz    = params[1].ival;
	int32_t data   = params[2].ival;
	int ret;
  
        struct spi_ioc_transfer tr = {
                .tx_buf = (unsigned long)&data,
                .rx_buf = (unsigned long)&data,
                .len = 1,
                .delay_usecs = 10,
                .speed_hz = freq_hz,
                .bits_per_word = 8,
        };

        ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
        if (ret < 1){
                ERROR("can't send spi message\n");
		data = -1;
	}	
	
	ret_cell.ival = data;
	return ret_cell;
}

Cell sproSpi_SpiBaseApi_initMode(SedonaVM* vm, Cell* params)
{
//	FTRACE();

	Cell       ret_cell;
	int fd   = params[0].ival;
	int ret; 
	int bits = 8;
	int mode = 0;

	if (params[1].ival) 
		mode |= SPI_CPOL;

	if (params[2].ival) 
		mode |= SPI_CPHA;

       /*
         * spi mode
         */
        ret = ioctl(fd, SPI_IOC_WR_MODE, &mode);
        if (ret == -1) {
                ERROR("can't set spi mode\n");
		goto ret_error;
	}

        ret = ioctl(fd, SPI_IOC_RD_MODE, &mode);
        if (ret == -1) {
                ERROR("can't get spi mode\n");
		goto ret_error;
	}

        /*
         * bits per word
         */
        ret = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
        if (ret == -1) {
                ERROR("can't set bits per word\n");
		goto ret_error;
	}

        ret = ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
        if (ret == -1) {
                ERROR("can't get bits per word\n");
		goto ret_error;
	}

ret_error: 
 	ret_cell.ival = ret;
	return ret_cell;
}


