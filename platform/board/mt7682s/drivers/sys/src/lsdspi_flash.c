#include "lsdspi_flash.h"
#include "type_def.h"
#include "hal_gpio.h"
#include <string.h>


#include <stdbool.h>

#include "hal_pinmux_define.h"
#include "hal_spi_master.h"
#include "hal_log.h"
#include "hal_gpt.h"
#include "os_mutex.h"



/*SPIM port (Platform dependent)*/
#define SPIM_PORT HAL_SPI_MASTER_0



//static BYTE CardType;            /* b0:MMC, b1:SDC, b2:Block addressing */
static uint32_t spi_mosi_pin = 0;
static uint8_t spi_mosi_pin_func_mosi = 0;
static uint8_t spi_mosi_pin_func_gpio = 0;



/*-----------------------------------------------------------------------*/
/* Transmit a byte to MMC via SPI  (Platform dependent)                  */
/*-----------------------------------------------------------------------*/
static void xmit_spi(BYTE dat)
{
    BYTE send_data;
    hal_spi_master_status_t status;

    send_data = dat;
    hal_pinmux_set_function((hal_gpio_pin_t)spi_mosi_pin, spi_mosi_pin_func_mosi);
    if (HAL_SPI_MASTER_STATUS_OK != (status = hal_spi_master_send_polling(SPIM_PORT, (uint8_t *)(&send_data), 1))) {
        log_hal_info("xmit_spi,SPI master send failed,status = %d \r\n", status);
    }
    hal_pinmux_set_function((hal_gpio_pin_t)spi_mosi_pin, spi_mosi_pin_func_gpio);
    hal_gpio_set_direction((hal_gpio_pin_t)spi_mosi_pin, HAL_GPIO_DIRECTION_OUTPUT);
    hal_gpio_set_output((hal_gpio_pin_t)spi_mosi_pin, HAL_GPIO_DATA_HIGH);
}

#if 0
/* Alternative macro to receive data fast */
static void xmit_spi_m(const BYTE *src, DWORD data_len)
{
    hal_spi_master_status_t status;

    hal_pinmux_set_function((hal_gpio_pin_t)spi_mosi_pin, spi_mosi_pin_func_mosi);
    if (HAL_SPI_MASTER_STATUS_OK != (status = hal_spi_master_send_polling(SPIM_PORT, (uint8_t *)src, data_len))) {
        log_hal_info("xmit_spi_m,SPI master send failed,status = %d \r\n", status);
    }
    hal_pinmux_set_function((hal_gpio_pin_t)spi_mosi_pin, spi_mosi_pin_func_gpio);
    hal_gpio_set_direction((hal_gpio_pin_t)spi_mosi_pin, HAL_GPIO_DIRECTION_OUTPUT);
    hal_gpio_set_output((hal_gpio_pin_t)spi_mosi_pin, HAL_GPIO_DATA_HIGH);
}
#endif
/*Recive one byte once.*/
static BYTE rcvr_spi(void)
{
    BYTE send_data[2];
    BYTE receive_data;
    hal_spi_master_send_and_receive_config_t config;
    hal_spi_master_status_t status;

    config.send_data = send_data;
    config.send_length = 0;
    config.receive_buffer = &receive_data;
    config.receive_length = 1;

    if (HAL_SPI_MASTER_STATUS_OK != (status = hal_spi_master_send_and_receive_polling(SPIM_PORT, &config))) {
        log_hal_info("rcvr_spi,SPI master send and receive failed, status = %d \r\n", status);
    }

    return receive_data;
}
#if 0
/*Recive mutil bytes once.*/
static void rcvr_spi_m(BYTE *dst, DWORD data_len)
{
    BYTE send_data;
    hal_spi_master_send_and_receive_config_t config;
    hal_spi_master_status_t status;

    config.send_data = &send_data;
    config.send_length = 0;
    config.receive_buffer = dst;
    config.receive_length = data_len;

    if (HAL_SPI_MASTER_STATUS_OK != (status = hal_spi_master_send_and_receive_polling(SPIM_PORT, &config))) {
        log_hal_info("rcvr_spi,SPI master send and receive failed,status = %d \r\n", status);
    }
}
#endif

os_mutex_handle_t lsdfile_xMutex = NULL;

void lsdfile_xMutex_init(void)
{
    lsdfile_xMutex = os_mutex_create();

    if(NULL == lsdfile_xMutex)  // Create fail
    {
        printf("lsdfile x init fail\r\n");
        return;
    }
    else
    {
        return;
    }
}


// return 0 not ready, 1 ok
char lsdspi_init_is_ok()
{
		if (lsdfile_xMutex == NULL)
			return 0;
		else
			return 1;
}

// return 0 ok , -1 fail
int lsdspi_take_lock()
{	
		int ret = -1;
		if (lsdfile_xMutex)
		{
				if ( 1 == os_mutex_get_lock(lsdfile_xMutex, 50) )
				{
						ret = 0;
				}
		}
		
		return ret;
}

// return 0 ok , -1 fail
int lsdspi_give_lock()
{
		int ret = -1;
		if (lsdfile_xMutex)
		{
				if ( 1 == os_mutex_release_lock(lsdfile_xMutex) )
				{
						ret = 0;
				}
		}
		
		return ret;
}


static u8 SPIx_WriteByte(u8 byte)
{
  SPI_Send_Dat(byte);
}


//
static u8 SPIx_ReadWriteByte(u8 byte)
{
    if (byte == 0xff)   // read byte is never 0xff
    {
        //return rcvr_spi();
        return SPI_Receiver_Dat();
    }
    else
    {
        //xmit_spi(byte);
         SPI_Send_Dat(byte);
    }
		
		return 0;
    
#if 0
    
    
  /* Loop while DR register in not emplty */
  while(SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_TXE) == RESET);

  /* Send byte through the SPI2 peripheral */
  SPI_I2S_SendData(SPI2, byte);

  /* Wait to receive a byte */
  while(SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_RXNE) == RESET);

  /* Return the byte read from the SPI bus */
  return SPI_I2S_ReceiveData(SPI2);
#endif
}

/*
 * wait for spi idle
 */
static void SPI_Flash_Wait_Busy(void)
{
    uint8_t state_reg=0x00;
    NotSelect_Flash();
    Select_Flash();
    SPIx_WriteByte(W25X_ReadStatusReg);
    state_reg = SPIx_ReadWriteByte(0xFF);
    while((state_reg&0x01) != 0)
    {
        //state_reg = spi_write_byte(Dummy_Byte);
        SPIx_WriteByte(W25X_ReadStatusReg);
        state_reg = SPIx_ReadWriteByte(0xFF);
    }
    NotSelect_Flash();

}


//SPI_FLASHдʹ��
//WEL��λ
static void SPI_FLASH_Write_Enable(void)
{
    Select_Flash();                            //Ƭѡʹ��
    SPIx_WriteByte(W25X_WriteEnable);      //����дʹ��ָ��
    NotSelect_Flash();                            //Ƭѡ����
}
#if 0
//SPI_FLASHд����
//WEL����
static void SPI_FLASH_Write_Disable(void)
{
    Select_Flash();                            //Ƭѡʹ��
    SPIx_WriteByte(W25X_WriteDisable);     //����д����ָ��
    NotSelect_Flash();                            //Ƭѡ����
}
#endif
//��ȡоƬID w25q16��ID��0xEF14
u8 SPI_Flash_ReadID(u8 *id)
{
		Select_Flash();
    SPIx_WriteByte(0x90);//����?��?����?ID?����?
    SPIx_WriteByte(0x00);
    SPIx_WriteByte(0x00);
    SPIx_WriteByte(0x00);
//  Temp|=spi_write_byte(0xFF)<<8;
//  Temp|=spi_write_byte(0xFF);
		
    id[0] = SPIx_ReadWriteByte(0xFF);
    id[1] = SPIx_ReadWriteByte(0xFF);
		NotSelect_Flash();
	
    return 0;
}


//��ȡSPI FLASH
//��ָ����ַ��ʼ��ȡָ����������
//pBuffer:���ݴ洢��
//ReadAddr:��ʼ��ȡ�ĵ�ַ(24bit)
//NumByteToRead:Ҫ��ȡ���ֽ���(���65535)
static void SPI_Flash_Read(u8* pBuffer,u32 ReadAddr,u16 NumByteToRead)
{
    u16 i;
    Select_Flash();                            //????
    SPIx_WriteByte(W25X_ReadData);         //??????
    SPIx_WriteByte((u8)((ReadAddr)>>16));  //?? 24Bit??
    SPIx_WriteByte((u8)((ReadAddr)>>8));
    SPIx_WriteByte((u8)ReadAddr);
    for(i=0;i<NumByteToRead;i++)
    {
        pBuffer[i]=SPIx_ReadWriteByte(0XFF);   //??????
    }
    NotSelect_Flash();                            //????
}



//SPI��һҳ�ڣ�0~65535����д������256���ֽ�����
//��ָ����ַ��ʼд�����256�ֽڵ�����
//pBuffer:���ݴ洢��
//WriteAddr:��ʼд��ĵ�ַ(24bit)
//NumByteToWrite:Ҫд����ֽ��������256����������Ӧ�ó�����ҳ��ʣ���ֽ���
static void SPI_Flash_Write_Page(u8* pBuffer,u32 WriteAddr,u16 NumByteToWrite)
{
    u16 i;


    SPI_FLASH_Write_Enable();                  //SET WEL
    Select_Flash();                            //Ƭѡʹ��
    SPIx_WriteByte(W25X_PageProgram);      //����дҳ����
    SPIx_WriteByte((u8)((WriteAddr)>>16)); //����24bit��ַ
    SPIx_WriteByte((u8)((WriteAddr)>>8));
    SPIx_WriteByte((u8)WriteAddr);
    for(i=0;i<NumByteToWrite;i++)SPIx_WriteByte(pBuffer[i]);//ѭ��д������
    NotSelect_Flash();                            //Ƭѡ����
    SPI_Flash_Wait_Busy();                     //�ȴ�д�����
 
}
void lsd_flash_write(u8* pBuffer,u32 WriteAddr,u16 NumByteToWrite)
{
	SPI_Flash_Write_Page(pBuffer,WriteAddr,NumByteToWrite);

}
//�޼���дSPI FLASH
//����ȷ����д�ĵ�ַ��Χ������ȫ��Ϊ0xFF�������ڷ�0xFF��д������ݽ�ʧ��
//�����Զ���ҳ����
//��ָ����ַ��ʼд��ָ�����ȵ����ݣ���Ҫȷ����ַ��Խ��
//pBuffer:���ݴ洢��
//WriteAddr:��ʼд��ĵ�ַ(24bit)
//NumByteToWrite:Ҫд����ֽ���(���65535)
//CHECK OK
static void SPI_Flash_Write_NoCheck(u8* pBuffer,u32 WriteAddr,u16 NumByteToWrite)
{
    u16 pageremain;
    pageremain=256-WriteAddr%256; //��ҳʣ����ֽ���
    if(NumByteToWrite<=pageremain)pageremain=NumByteToWrite;//������256���ֽ�
    while(1)
    {
    	//printf("write page:%x,len:%d,buf:%s\r\n",WriteAddr,pageremain,pBuffer);
        SPI_Flash_Write_Page(pBuffer,WriteAddr,pageremain);
        if(NumByteToWrite==pageremain)break;//д�������
        else //NumByteToWrite>pageremain
        {
            pBuffer+=pageremain;
            WriteAddr+=pageremain;

            NumByteToWrite-=pageremain;           
            if(NumByteToWrite>256)pageremain=256; 
            else pageremain=NumByteToWrite;      
        }
    }
}


void SPI_Flash_Erase_Sector(u32 Dst_Addr)
{
	//printf("%d\r\n", Dst_Addr);
    Dst_Addr*=FLASH_SECTOR_SIZE;
	//printf("0x%08X\r\n", Dst_Addr);
    SPI_FLASH_Write_Enable();                  //SET WEL
    SPI_Flash_Wait_Busy();
    Select_Flash();                            //Ƭѡʹ��
    SPIx_WriteByte(W25X_SectorErase);      //������������ָ��
    SPIx_WriteByte((u8)((Dst_Addr)>>16));  //����24Bit��ַ
    SPIx_WriteByte((u8)((Dst_Addr)>>8));
    SPIx_WriteByte((u8)Dst_Addr);
    NotSelect_Flash();                            //ȡ��Ƭѡ
    SPI_Flash_Wait_Busy();                     //�ȴ��������
}


static u8 SPI_FLASH_BUF[4096];
static void SPI_Flash_Write(u8* pBuffer,u32 WriteAddr,u16 NumByteToWrite)
{
    u32 secpos;
    u16 secoff;
    u16 secremain;
    u16 i;
    secpos=WriteAddr/4096;//
    secoff=WriteAddr%4096;
    secremain=4096-secoff;
    if(NumByteToWrite<=secremain)secremain=NumByteToWrite;
    while(1)
    {
        SPI_Flash_Read(SPI_FLASH_BUF,secpos*4096,4096);
        for(i=0;i<secremain;i++)
        {
            if(SPI_FLASH_BUF[secoff+i]!=0XFF)break;
        }
        if(i<secremain)//��Ҫ����
        {
            SPI_Flash_Erase_Sector(secpos);//������������
            for(i=0;i<secremain;i++)       //����
            {
                SPI_FLASH_BUF[i+secoff]=pBuffer[i];
            }
            SPI_Flash_Write_NoCheck(SPI_FLASH_BUF,secpos*4096,4096);//д����������

        }else SPI_Flash_Write_NoCheck(pBuffer,WriteAddr,secremain);//д�Ѿ������˵ģ�ֱ��д������ʣ��ռ�
        if(NumByteToWrite==secremain)break;// д�����
        else//д��δ����
        {
            secpos++;//������ַ��1
            secoff=0;//ƫ�Ƶ�ַΪ0

            pBuffer+=secremain;  //ָ��ƫ��
            WriteAddr+=secremain;//д��ַƫ��
            NumByteToWrite-=secremain;              //�ֽ����ݼ�
            if(NumByteToWrite>4096)secremain=4096;  //��һ����������д����
            else secremain=NumByteToWrite;          //��һ����������д��
        }
    }
}


//Sector Read
void W25X_Read_Sector(uint32_t nSector, u8* pBuffer)
{
    uint16_t i;
    //������תΪ��ַ
    nSector *= FLASH_SECTOR_SIZE;

    Select_Flash();
    SPIx_WriteByte(W25X_ReadData);
    SPIx_WriteByte(((nSector & 0xFFFFFF) >> 16));
    SPIx_WriteByte(((nSector & 0xFFFF) >> 8));
    SPIx_WriteByte(nSector & 0xFF);

    for(i=0;i<FLASH_SECTOR_SIZE;i++)
    {
      pBuffer[i] = SPIx_ReadWriteByte(0xFF);
    }
    NotSelect_Flash();
    SPI_Flash_Wait_Busy();
}

//Sector Write
void W25X_Write_Sector(uint32_t nSector, u8* pBuffer)
{
    int i,j;
    //������תΪ��ַ
    nSector *= FLASH_SECTOR_SIZE;

    //һ��������Ҫ����ҳ
    for(j=0;j<FLASH_PAGES_PER_SECTOR;j++)
    {
        SPI_FLASH_Write_Enable();                  //SET WEL

        Select_Flash();
        SPIx_WriteByte(W25X_PageProgram);
        SPIx_WriteByte(((nSector & 0xFFFFFF) >> 16));
        SPIx_WriteByte(((nSector & 0xFFFF) >> 8));
        SPIx_WriteByte(nSector & 0xFF);

        for(i=0;i<FLASH_PAGE_SIZE;i++)
        SPIx_WriteByte(pBuffer[i]);

        pBuffer += FLASH_PAGE_SIZE;
        nSector += FLASH_PAGE_SIZE;

        NotSelect_Flash();
        SPI_Flash_Wait_Busy();
    }
}




//------------------below safe api to user---------------------//

//4Kbytes  size of Sector
//16 sectors = 1 block
//W25Q16
//capality 2MByte( 32 Block,512 Sector)

/*
 * lsdspi_flash_init
 *
 * must call this function before call lsdfile_xxx api or lsdspi_xxx api 
 *
 * return NumByteToRead success, -1 fail
 */
void lsdspi_flash_init(void)
{    
	SPI_Init();
	lsdfile_xMutex_init();
}

/*
 * lsdspi_flash_read
 *
 * return NumByteToRead success, -1 fail
 */
int lsdspi_flash_read(u8* pBuffer,u32 ReadAddr,u16 NumByteToRead)
{
		//if (0 == lsdspi_take_lock())
		{
				SPI_Flash_Read(pBuffer, ReadAddr, NumByteToRead);
				
		//		lsdspi_give_lock();
			
				return NumByteToRead;
		}
		
		//return -1;
}

/*
 * lsdspi_flash_write
 * no need to earse before write.
 *
 * return NumByteToWrite success, -1 fail
 */
int lsdspi_flash_write(u8* pBuffer,u32 WriteAddr,u16 NumByteToWrite)
{
		//if (0 == lsdspi_take_lock())
		{
				SPI_Flash_Write(pBuffer, WriteAddr, NumByteToWrite);
				
				//lsdspi_give_lock();
			
				return NumByteToWrite;
		}	
		
		//return -1;
}

/*
 * lsdspi_flash_write
 * need to earse before write.
 *
 * return NumByteToWrite success, -1 fail
 */
int lsdspi_flash_write_nocheck(u8* pBuffer,u32 WriteAddr,u16 NumByteToWrite)
{
	//	if (0 == lsdspi_take_lock())
		{
				SPI_Flash_Write_NoCheck(pBuffer, WriteAddr, NumByteToWrite);
				
		//		lsdspi_give_lock();
			
				return NumByteToWrite;
		}	
		
		//return -1;	
		
}

/*
 * lsdspi_flash_erase_sector
 * Dst_Addr: range 0~511 for w25x16
 *
 * cost 150ms to erase a sector.
 *
 * return 0 success, -1 fail
 */
int lsdspi_flash_erase_sector(u32 Dst_Addr)
{
		if (Dst_Addr > 511)
		{
				return -1;
		}
		
		if (0 == lsdspi_take_lock())
		{
				SPI_Flash_Erase_Sector(Dst_Addr);
				
				lsdspi_give_lock();
			
				return 0;
		}
		
		return -1;
		
}

/**
 * lsdspi_flash_addr_write
 *
 * @param FlashAddress
 * @param Data
 * @param DataLength
 * @param inc   if FlashAddress increase by DataLength.
 *
 * @return int -1 error, DataLength success
 */
int lsdspi_flash_addr_write(__IO uint32_t* FlashAddress, uint8_t* Data ,uint16_t DataLength, uint8_t inc)
{
		uint16_t size = DataLength;
		uint32_t write_bytes, bytes, dst_address;

		// Write sectors
		write_bytes = 0;
		dst_address = *FlashAddress;

		if (lsdspi_take_lock() == 0)
		{
        while(size){
            if(size > 512)  bytes = 512;
            else            bytes = size;
            SPI_Flash_Write((char*)Data + write_bytes, dst_address + write_bytes, bytes);

            write_bytes += bytes;
            size -= bytes;
            *FlashAddress += bytes;
        }
        if(!inc){
            *FlashAddress = dst_address;
        }

				lsdspi_give_lock();
        return DataLength;
		}
		
		return 0;
}
#if 0
void spi_test()
{
  char id[2]={0};
  char buf[512]="1234567";
  char read_buf[512];
    printf("bbbbbbbbbbb id %x-%x\n\r", id[0],id[1]);
  lsdspi_flash_init();
    printf( "cccccccccccc id %x-%x\n\r", id[0],id[1]);
//  while(1)
    SPI_Flash_ReadID(id);
 printf( "flash id %x-%x\n\r", id[0],id[1]);

 // flash_delay_us(1000*3000);
	lsdspi_flash_erase_sector(2);
	lsdspi_flash_erase_sector(3);
	lsdspi_flash_erase_sector(4);

	memset(read_buf,0,sizeof(read_buf));
    SPI_Flash_Write_NoCheck(buf,0xff00,sizeof(buf));
  lsdspi_flash_read(read_buf,0xff00,100);
  printf("read info: %s\n\r", read_buf);
  printf("===========%s=========\r\n",read_buf);
  
  char buffer[20];
  for(int i=0;i<10;i++)
  	buffer[i]=0xff;
  for(int i=0;i<10;i++)
  	buffer[i+10]=0xa0+i;
  lsdspi_flash_write(buffer,4096,sizeof(buffer));
  lsdspi_flash_read(read_buf,4096,sizeof(buffer));
  for(int i=0;i<20;i++)
  	printf("%x-",read_buf[i]);
  printf("\r\n");
}
#endif

