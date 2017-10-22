/*
 * Routines based on twitest.c by Joerg Wunsch
 * Modified for optiboot by Krister W. <kisse66@hobbylabs.org>
 *
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <joerg@FreeBSD.ORG> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.        Joerg Wunsch
 * ----------------------------------------------------------------------------
 *
 * This file is included from optiboot.c so that the I2C routines don't make the
 * original too long.
 *
 * Until someone has time to optimize unfortunately these routines use
 * so much space, that 1kB is no longer enough for the bootloader.
 */

#define MAX_ITER 200  //< maximum retries
#define MAX_TIMEOUT 250  //< maximum timeout while waiting for I2C response
uint8_t twst;

//uint8_t StartTwiTransfer(uint8_t dr)
//{
//    TWDR = dr;
//    TWCR = _BV(TWINT) | _BV(TWEN); /* clear interrupt to start transmission */
//    while ((TWCR & _BV(TWINT)) == 0) ; /* wait for transmission */
//    return TW_STATUS;
//}

static inline void EEPROM_init()
{
    /* initialize TWI clock: 100 kHz clock, TWPS = 0 => prescaler = 1 */
#if defined(TWPS0)
    /* has prescaler (mega128 & newer) */
    TWSR = 0;
#endif

#if F_CPU < 4000000UL
    TWBR = 10;
#else
    TWBR = ((F_CPU / 100000UL) - 16) / 2;
#endif

    //TWDR = 0xFF; 
    TWCR = (1<<TWEN) | (0<<TWEA);
}

static inline uint8_t FLASH_readByte(uint32_t eeaddr)
{
    uint8_t twcr, n = 0, m = 0, buf=0xFF;

  /*
   * Note [8]
   * First cycle: master transmitter mode
   */
restart:
    if (n++ >= MAX_ITER)
        return 0xFF;
begin:

    m=0;
    TWCR = _BV(TWINT) | _BV(TWSTA) | _BV(TWEN); /* send start condition */
    while (((TWCR & _BV(TWINT)) == 0) && (m++ < MAX_TIMEOUT)) ; /* wait for transmission */
	if (m > MAX_TIMEOUT) {
#ifdef DEBUG_ON
		putch('1');
#endif
		return 0xFF; 
	}
	switch ((twst = TW_STATUS)) {
        case TW_REP_START:		// OK, but should not happen 
        case TW_START:
            break;
        case TW_MT_ARB_LOST:	// Note [9] 
            goto begin;
      default:
            return 0xFF;		// error: not in start condition 
				      // NB: do /not/ send stop condition
    }

    /* Note [10] */
    /* send SLA+W */
    m = 0;
    TWDR = I2C_EEPROM_ADDR | TW_WRITE;
    TWCR = _BV(TWINT) | _BV(TWEN); /* clear interrupt to start transmission */
    while (((TWCR & _BV(TWINT)) == 0) && (m++ < MAX_TIMEOUT)) ; /* wait for transmission */
    if (m > MAX_TIMEOUT) {
#ifdef DEBUG_ON
		putch('2');
#endif
		goto error;
	}
	switch ((twst = TW_STATUS)) {
        case TW_MT_SLA_ACK:
            break;

        case TW_MT_SLA_NACK:	/* nack during select: device busy writing */
				    /* Note [11] */
            goto restart;

        case TW_MT_ARB_LOST:	/* re-arbitrate */
            goto begin;

        default:
            goto error;		/* must send stop condition */
    }

    m = 0;
    TWDR = ((uint16_t)eeaddr >> 8);		/* 16-bit word address device, send high 8 bits of addr */
    TWCR = _BV(TWINT) | _BV(TWEN); /* clear interrupt to start transmission */
    while (((TWCR & _BV(TWINT)) == 0) && (m++ < MAX_TIMEOUT)); /* wait for transmission */
	if (m > MAX_TIMEOUT) {
#ifdef DEBUG_ON
		putch('3');
#endif
		goto error;
	}
	switch ((twst = TW_STATUS)) {
        case TW_MT_DATA_ACK:
            break;
        case TW_MT_DATA_NACK:
            goto quit;
        case TW_MT_ARB_LOST:
            goto begin;
        default:
            goto error;		/* must send stop condition */
    }

    m = 0;
    TWDR = eeaddr;		/* low 8 bits of addr */
    TWCR = _BV(TWINT) | _BV(TWEN); /* clear interrupt to start transmission */
    while (((TWCR & _BV(TWINT)) == 0) && (m++ < MAX_TIMEOUT)) ; /* wait for transmission */
	if (m > MAX_TIMEOUT){
#ifdef DEBUG_ON
		putch('4');
#endif
		goto error;
	}
	switch ((twst = TW_STATUS)) {
        case TW_MT_DATA_ACK:
            break;
        case TW_MT_DATA_NACK:
            goto quit;
        case TW_MT_ARB_LOST:
            goto begin;
        default:
            goto error;		/* must send stop condition */
    } 

    /*
    * Note [12]
    * Next cycle(s): master receiver mode
    */
    m = 0;
    TWCR = _BV(TWINT) | _BV(TWSTA) | _BV(TWEN); /* send (rep.) start condition */
    while (((TWCR & _BV(TWINT)) == 0) && (m++ < MAX_TIMEOUT)) ; /* wait for transmission */
	if (m > MAX_TIMEOUT) {
#ifdef DEBUG_ON
		putch('5');
#endif
		goto error;
	}
	switch ((twst = TW_STATUS)) {
        case TW_START:		// OK, but should not happen 
        case TW_REP_START:
            break;
        case TW_MT_ARB_LOST:
            goto begin;
        default:
            goto error;
    }

    /* send SLA+R */
    m = 0;
    TWDR = I2C_EEPROM_ADDR | TW_READ;
    TWCR = _BV(TWINT) | _BV(TWEN); /* clear interrupt to start transmission */
    while (((TWCR & _BV(TWINT)) == 0) && (m++ < MAX_TIMEOUT)); /* wait for transmission */
	if (m > MAX_TIMEOUT) {
#ifdef DEBUG_ON
		putch('6');
#endif
		goto error;
	}
	switch ((twst = TW_STATUS)) {
        case TW_MR_SLA_ACK:
            break;
        case TW_MR_SLA_NACK:
            goto quit;
        case TW_MR_ARB_LOST:
            goto begin;
        default:
          goto error;
    }

    m = 0;
    twcr = _BV(TWINT) | _BV(TWEN); /* send NAK this time */
    TWCR = twcr;		/* clear int to start transmission */
    while (((TWCR & _BV(TWINT)) == 0) && (m++ < MAX_TIMEOUT)); /* wait for transmission */
	if (m > MAX_TIMEOUT) {
#ifdef DEBUG_ON
		putch('7');
#endif
		goto error;
	}
	switch ((twst = TW_STATUS)) {
	      case TW_MR_DATA_ACK:
	      case TW_MR_DATA_NACK:
	          buf = TWDR;
	          break;
	      default:
	          goto error;
	  }

quit:
    /* Note [14] */
    TWCR = _BV(TWINT) | _BV(TWSTO) | _BV(TWEN); /* send stop condition */
    return buf;

error:
    goto quit;
}

// used to invalidate EEPROM after update, writes 8 0xFFs
// (with SPI flash a page erase is used)
static inline void EEPROM_invalidate()
{
    uint8_t n = 0, m = 0;

restart:
    if (n++ >= MAX_ITER)
        return;
begin:

    /* Note [15] */
    m = 0;
    TWCR = _BV(TWINT) | _BV(TWSTA) | _BV(TWEN); /* send start condition */
    while (((TWCR & _BV(TWINT)) == 0) && (m++ < MAX_TIMEOUT)) ; /* wait for transmission */
    if (m > MAX_TIMEOUT) {
#ifdef DEBUG_ON
		putch('A');
#endif
		return;
	}
    switch ((twst = TW_STATUS)) {
	      case TW_REP_START:		/* OK, but should not happen */
	      case TW_START:
	          break;
	      case TW_MT_ARB_LOST:
	          goto begin;
	      default:
	          return;		/* error: not in start condition */
	                  /* NB: do /not/ send stop condition */
    }

    /* send SLA+W */
    m = 0;
    TWDR = I2C_EEPROM_ADDR;
    TWCR = _BV(TWINT) | _BV(TWEN); /* clear interrupt to start transmission */
    while (((TWCR & _BV(TWINT)) == 0) && (m++ < MAX_TIMEOUT)) ; /* wait for transmission */
    if (m > MAX_TIMEOUT) {
#ifdef DEBUG_ON
		putch('B');
#endif
		goto error;
	}
    switch ((twst = TW_STATUS)) {
	      case TW_MT_SLA_ACK:
	          break;
	      case TW_MT_SLA_NACK:	/* nack during select: device busy writing */
	          goto restart;
	      case TW_MT_ARB_LOST:	/* re-arbitrate */
	          goto begin;
	      default:
	          goto error;		/* must send stop condition */
    }

    TWDR = 0; m = 0;
    TWCR = _BV(TWINT) | _BV(TWEN); /* clear interrupt to start transmission */
    while (((TWCR & _BV(TWINT)) == 0) && (m++ < MAX_TIMEOUT)) ; /* wait for transmission */
    if (m > MAX_TIMEOUT) {
#ifdef DEBUG_ON
		putch('C');
#endif
		goto error;
	}
    switch ((twst = TW_STATUS)) {
	      case TW_MT_DATA_ACK:
	          break;
	      case TW_MT_DATA_NACK:
	          goto quit;
	      case TW_MT_ARB_LOST:
	          goto begin;
	      default:
	          goto error;		/* must send stop condition */
    }

    m = 0;
    TWDR = 0;		/* low 8 bits of addr */
    TWCR = _BV(TWINT) | _BV(TWEN); /* clear interrupt to start transmission */
    while (((TWCR & _BV(TWINT)) == 0) && (m++ < MAX_TIMEOUT)); /* wait for transmission */
    if (m > MAX_TIMEOUT) {
#ifdef DEBUG_ON
		putch('D');
#endif
		goto error;
	}
    switch ((twst = TW_STATUS)) {
	      case TW_MT_DATA_ACK:
	          break;
	      case TW_MT_DATA_NACK:
	          goto quit;
	      case TW_MT_ARB_LOST:
	          goto begin;
	      default:
	          goto error;		/* must send stop condition */
    }

    // write 8 times 0xFF
    for (n=8; n ; n--) {
	      TWDR = 0xFF; m = 0;
	      TWCR = _BV(TWINT) | _BV(TWEN); /* start transmission */
	      while (((TWCR & _BV(TWINT)) == 0)  && (m++ < MAX_TIMEOUT)); /* wait for transmission */
	      if (m > MAX_TIMEOUT) {
#ifdef DEBUG_ON
			putch('E');
#endif
			goto error;
		  }
	      switch ((twst = TW_STATUS)) {
		        case TW_MT_DATA_NACK:
		            goto error;		/* device write protected -- Note [16] */
		        case TW_MT_DATA_ACK:
		            break;
		        default:
		            goto error;
	      }
    }

quit:
    TWCR = _BV(TWINT) | _BV(TWSTO) | _BV(TWEN); /* send stop condition */
    // add delay to complete write?
    return;

error:
    goto quit;
}
