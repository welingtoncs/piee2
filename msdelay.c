/*
 * File:   msdelay.c
 * Author: welingtonsouza
 *
 * Created on December 7, 2025, 9:54 AM
 */


#include <xc.h>
#include <stdio.h>
#include "config_header.h"
#include <pic18f4550.h>

void MSdelay(unsigned int val)
{
 unsigned int i,j;
 for(i=0;i<val;i++)
     for(j=0;j<165;j++);  
}
