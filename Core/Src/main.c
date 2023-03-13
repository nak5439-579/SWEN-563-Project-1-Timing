#include "stm32l476xx.h"
#include "SysClock.h"
#include "UART.h"
#include "LED.h"

#include <string.h>
#include <stdio.h>

char buffer[BufferSize];

int main(void){
	int lower_limit = 950;
	int upper_limit = lower_limit + 100;
	int bins[100];//bins for use to make the histogram
	int	n;
	int num_recieved = 0;//the number of pulses received
	int post_not_passed = 1;//flag to indicate if the POST is running
	char buffer[64];//generic input buffer
	uint32_t capture_1;
	uint32_t capture_2;
	int outliers = 0;
	uint32_t delta;
	int bounds_not_changed = 1;
	uint8_t ch_in;

	SysClock_Init(); 		// Switch System Clock = 80 MHz
	UART2_Init();			// set UART to 9600/N81
	LED_Init();				// set up GPIO to drive LEDs



	//Timer selection and config
	RCC->APB1ENR1 |= RCC_APB1ENR1_TIM2EN;//enable clock for TIM2
	TIM2->PSC = 79;//loading prescale value: 79+1=80 80MHz/80=1MHz
	TIM2->EGR |= TIM_EGR_UG;//forcing the PSC to load by creating an update event
	//-----------------

	//GPIO and AF setup
	RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;//enable clock on GPIOA
	GPIOA->MODER &= ~GPIO_MODER_MODE0_Msk;//clear GPIOA Mode0 (pin A0)
	GPIOA->MODER |= GPIO_MODER_MODE0_1;//set mode of GPIOA 0 to AF mode
	GPIOA->AFR[0] |= GPIO_AFRL_AFSEL0_0;//set GPIOA0 to AF1
	//-----------------

	//TIM2 capture config
	TIM2->CCER &= ~TIM_CCER_CC1E;//disable CC output
	TIM2->CCMR1 &= ~TIM_CCMR1_CC1S_Msk;//clear the bits
	TIM2->CCMR1 |= TIM_CCMR1_CC1S_0;//set channel 1(?) as input
	TIM2->ARR = ~0;
	TIM2->CCER |= TIM_CCER_CC1E;//reenable CC output
	TIM2->CR1 |= TIM_CR1_CEN;


	//POST loop


	char *msg = "\r\nPOST is now running...";
	USART_Write(USART2, (uint8_t *)msg, strlen(msg));
	uint32_t end_count = TIM2->CNT + 100000;
	while(post_not_passed){

		while(TIM2->CNT < end_count){
			//this needs to wait until it receives a rising edge within 100ms

			if(TIM2->SR & TIM_SR_CC1IF_Msk){
				post_not_passed = 0;
			}
		}

		if(post_not_passed){
			msg = "\r\nPOST unsuccessful, try again? (y/n)";
			USART_Write(USART2, (uint8_t *)msg, strlen(msg));

			ch_in = USART_Read(USART2);
			USART_Write(USART2, &ch_in, 1);

			if(ch_in == 'y' || ch_in == 'Y'){
				msg = "\r\nRetrying...";
				USART_Write(USART2, (uint8_t *)msg, strlen(msg));
				end_count = TIM2->CNT + 100000;
			}
			else{
				msg = "\r\nWell, the documentation didn't tell us what to do if you didn't want to retry this, so we're just going to sit here now. Have fun with that!";
				USART_Write(USART2, (uint8_t *)msg, strlen(msg));
				while(1){

				}
			}
		}

	}



	msg = "\r\nPOST has completed successfully.";
	USART_Write(USART2, (uint8_t *)msg, strlen(msg));


	//POST has completed, and this is now normal opperation
	//beginning normal opperation
	n = sprintf((char *)buffer, "\r\nThe lower limit is: %d\r\nThe upper limit is: %d", lower_limit, upper_limit);
	USART_Write(USART2, (uint8_t *)buffer, n);

	msg = "\r\nWould you like to change the bounds? (y/n): ";
	USART_Write(USART2, (uint8_t *)msg, strlen(msg));

	ch_in = USART_Read(USART2);
	USART_Write(USART2, &ch_in, 1);

	while(1){
		if(ch_in == 'y' || ch_in == 'Y'){
			msg = "\r\nPlease enter the new lower bound: ";
			USART_Write(USART2, (uint8_t *)msg, strlen(msg));
			char *buf_ptr = buffer;					// initialize ptr to start of buffer
			for(int ii=0; ii<sizeof(buffer); ii++) {
				ch_in = USART_Read(USART2);
				USART_Write(USART2, &ch_in, 1);		// echo char back to terminal
				*buf_ptr++ = ch_in;					// put character into buffer, increment pointer
				if ((ch_in == '\n')  || (ch_in == '\r')) {
					break;							// either newline or carriage return ends input
				}
			}
			sscanf(buffer, "%d", &n);//converting user ASCII to an int
			lower_limit = n;
			upper_limit = lower_limit+100;
			n = sprintf((char *)buffer, "\r\nThe lower limit is now: %d, and the upper limit is now: %d", lower_limit, upper_limit);
			USART_Write(USART2, (uint8_t *)buffer, n);
			bounds_not_changed = 1;
		}


		while(bounds_not_changed){
			msg = "\r\n\nWaiting to begin capture sequence. Press enter to begin";
			USART_Write(USART2, (uint8_t *)msg, strlen(msg));
			ch_in = 'a';
			while(ch_in != '\r' && ch_in != '\n'){
				ch_in = USART_Read(USART2);
			}

			//reset data storage in case the capture is being repeated
			num_recieved = 0;
			outliers = 0;
			for(int i = 0;i<100;i++){
				bins[i] = 0;
			}
			msg = "\r\nCapturing input...";
			USART_Write(USART2, (uint8_t *)msg, strlen(msg));

			while (num_recieved < 1000) {

				while(!(TIM2->SR & TIM_SR_CC1IF_Msk)){

				}
				capture_1 = TIM2->CCR1;
				while(!(TIM2->SR & TIM_SR_CC1IF_Msk)){

				}
				capture_2 = TIM2->CCR1;


				if(capture_1 < capture_2){
					delta = capture_2 - capture_1;
				}
				else
				{
					delta = capture_1 - capture_2;
				}

				if(delta >= lower_limit && delta < upper_limit){
					bins[delta-lower_limit] += 1;
				}
				else{
					outliers += 1;
				}
				num_recieved++;
			}

			msg = "\r\nData capture completed. Results:";
			USART_Write(USART2, (uint8_t *)msg, strlen(msg));

			for(int i = 0;i < 100;i++){
				if(bins[i] != 0){
					n = sprintf((char *)buffer, "\r\n%d  %d", i+lower_limit, bins[i]);
					USART_Write(USART2, (uint8_t *)buffer, n);
				}
			}

			n = sprintf((char *)buffer, "\r\nOutliers:  %d", outliers);
			USART_Write(USART2, (uint8_t *)buffer, n);

			msg = "\r\nWould you like to change the bounds before the next run? (y/n): ";
			USART_Write(USART2, (uint8_t *)msg, strlen(msg));

			ch_in = USART_Read(USART2);
			USART_Write(USART2, &ch_in, 1);

			if(ch_in == 'y' || ch_in == 'Y'){
				bounds_not_changed = 0;
			}
		}

	}
	return 0;
}
