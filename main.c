#include "system_stm32f10x.h"
#include "stm32f10x_conf.h"

void pwm_init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	/* GPIOA Configuration: Channel 1, 2 and 3 as alternate function push-pull */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);	// PWM initialization code goes here

	
}

int main(void)
{
	SystemInit();


	while(1);
	return 0;
}
