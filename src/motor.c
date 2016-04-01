/*
 * Generic Electric Unicycle firmware
 *
 * Copyright (C) Casainho, 2015.
 *
 * Released under the GPL License, Version 3
 */

#include "stm32f10x.h"
#include "hall_sensor.h"
#include "bldc.h"
#include "pwm.h"
#include "main.h"
#include "bldc.h"

static unsigned int _motor_speed_target = 0;

unsigned int motor_get_speed (void)
{
  unsigned int motor_speed;
//  motor_speed = 1.0 / ((get_hall_sensors_us () * 6.0) / 1000000.0);
  return motor_speed;
}

void motor_set_speed (unsigned int speed)
{
  (void) speed;
}

void motor_start (void)
{
  TIM_CtrlPWMOutputs (TIM1, ENABLE); // enable now the signals to motor

  TIM_Cmd (TIM1, ENABLE); // start now the PWM / motor control

  machine_state = RUNNING;
}

void motor_coast (void)
{
  // TODO stop_adc_max_current_management ();
  TIM_CtrlPWMOutputs (TIM1, DISABLE); // PWM Output Disable
  machine_state = COAST;
}

void motor_set_duty_cycle (int value)
{
  pwm_set_duty_cycle (value);
}

unsigned int motor_get_current (void)
{
  unsigned int adc_current;
  unsigned int current;


  adc_current = adc_get_motor_current_value (); // TODO needs work for correct value

  /*
   * 1A = 0.0385V
   * (350W nominal motor power) 5.8A = 0.2233V
   * (350*1.5W peak motor power ??) = 0.335V
   *
   * 12 bits ADC; 4096 steps
   * 3.3V / 4096 = 0.0008
   *
   */
   current = adc_current * 0.0008;

   return current;
}

void TIM1_BRK_IRQHandler (void)
{
  TIM_CtrlPWMOutputs (TIM1, DISABLE); //disable PWM signals
  TIM_ITConfig (TIM1, TIM_IT_Break, ENABLE); //disable this interrupt
  machine_state = OVER_CURRENT;

  // clear interrupt flag
  TIM_ClearITPendingBit (TIM1, TIM_IT_Break);

  while (1) ;
}

void brake_init (void)
{
  TIM_BDTRInitTypeDef TIM_BDTRInitStructure;
  TIM_BDTRInitStructure.TIM_OSSRState = TIM_OSSRState_Disable;
  TIM_BDTRInitStructure.TIM_OSSIState = TIM_OSSIState_Disable;
  TIM_BDTRInitStructure.TIM_LOCKLevel = TIM_LOCKLevel_OFF;
  TIM_BDTRInitStructure.TIM_DeadTime = 0;
  TIM_BDTRInitStructure.TIM_Break = TIM_Break_Enable;
  TIM_BDTRInitStructure.TIM_BreakPolarity = TIM_BreakPolarity_Low;
  TIM_BDTRInitStructure.TIM_AutomaticOutput = TIM_AutomaticOutput_Disable;
  TIM_BDTRConfig(TIM1, &TIM_BDTRInitStructure);

  NVIC_InitTypeDef NVIC_InitStructure;
  NVIC_InitStructure.NVIC_IRQChannel = TIM1_BRK_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&NVIC_InitStructure);

  /* Enable Break interrupt */
  TIM_ITConfig (TIM1, TIM_IT_Break, ENABLE);
}

void motor_set_direction (unsigned int direction)
{
  bldc_set_direction (direction);
}

unsigned int motor_get_direction (void)
{
  return bldc_get_direction ();
}

// Called at every 10ms
void motor_manage_speed (void)
{
  float motor_speed;
  float error;
  float kp = 0.1;
  float	p_term = 0;
  float ki = 0.005;
  float	i_term = 0;
  float out = 0;

/*
 * speed (meters/hour) = 4032 / (138 * hall_sensor_period)
 * 4032 is the meters per hour (4.032km) that one rotation per second for wheel of 14''
 * 138 is the 46 magnets * 3 hall sensors
 * hall_sensor_period is measured in seconds
 * Examples:
 * 4032 / (138 * 0.000833) = 35075 --> ~35km/h
 * 4032 / (138 * 0.0059) = 35075 --> ~5km/h (the walking speed)
 *
 * speed (meters/hour) = 4032x10^6 / (138 * hall_sensor_period_us)
 * speed (meters/hour) = 29217391.3 / hall_sensor_period_us
 * speed (meters/hour) = 2921739.13 / hall_sensor_period_10us
 */

  motor_speed = 2921739.13 / ((float) get_hall_sensors_10us ());

  error = (float) (_motor_speed_target - motor_speed); // get the error from the target to current value
  p_term = error * kp;
  i_term += error * ki;
//  out = p_term;
  out = p_term + i_term;
  pwm_set_duty_cycle (out); // 0 --> 1000;
}

// Speed in meter per hour
void motor_set_motor_speed (unsigned int value)
{
  if (value >= MOTOR_MAX_SPEED)
  {
    value = MOTOR_MAX_SPEED;
  }
  else if (value <= MOTOR_MIN_SPEED)
  {
    value = MOTOR_MIN_SPEED;
  }

  _motor_speed_target = value;
}

