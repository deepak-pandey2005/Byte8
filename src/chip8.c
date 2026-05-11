#include "chip8.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


void chip8_init(Chip8 *chip8){
      memset(chip8,0,sizeof(Chip8)); //this sets the entire memory to 0
    /* we can use memset to reset the memory to 0 or 
       
       for(int i = 0; i < 2048; ++i)
		       gfx[i] = 0;

       we can use like this for all other variables in the struct which we are using as a emulator information*/

    Chip8->pc = 0x200; // 0x200-0xFFF - Program ROM and work RAM
}

void chip8_load_rom(Chip8 *chip8 , const char *filename)
{
    printf("LOADING : %s \n", filename);
    FILE *file = fopen(filename, "rb"); //opens a file

    if (!file){
        printf("FAILED TO OPEN ROM \n");
        exit(1);
    }
    fseek(file,0, SEEK_END); //move cursor to end of file
    long size = ftell(file); //get the file size
    rewind(file); // now it returns to the beggining 

    //allocate memory to contain the whole file
    uint8_t *buffer = malloc(size);
    if (buffer == NULL){
        fputs("MEMORY ERROR",stderr);
        exit(1);
    }

    //copy the whole file into the buffer
    size_t result = fread (buffer,1,size,file);
    if(result != size){
        fputs("READING ERROR",stderr);
        exit(1);
    

    //copy buffer to chip8 memory
    if(size <=(4096-512)){
        for(int i = 0 ; i<size ; i++)
           Chip8->memory[i+ 512] = buffer[i];
    }
    else
    {
        printf("ERROR ;ROM too big");
    }

    // close file ,free buffer
    fclose(file);
    free(buffer);
}

void chip8_cycle(Chip8 *chip8)
{
    // fetch opcode
    Chip8->opcode = Chip8->memory[pc] << 8 | memory[pc + 1]; 
    /* as one opcode is two byte long we will need to fetch two
    * succesive byte and merge them to get an actual opcode .*/

    Chip8->pc +=2;
    // decode opcode
  switch (Chip8->opcode & 0xF000)
  {
        case 0x0000:
        {
            switch (chip8->opcode)
            {
                case 0x00E0:
                {
                    memset(chip8->gfx, 0, sizeof(chip8->gfx));
                    chip8->draw_flag = true;
                    break;
                }

                case 0x00EE:
                {
                    chip8->sp--;
                    chip8->pc = chip8->stack[chip8->sp];
                    break;
                }
            }
            break;
        }

  
        case 0x1000:
        {
            uint16_t address = Chip8->opcode & 0x0FFF;
            Chip8->pc = address;
            break;
        } /* this opcode implement the chip8 1NNN instruction which means jump to address NNN */

        case 0x2000:
        {
            Chip8->stack[Chip->sp] = Chip->pc;
            ++sp;
            Chip8->pc = Chip8->opcode & 0x0FFF;
            break;
        } // this opcode calls the sub routine at address NNN

    case 0x6000;
    {
       uint8_t vx = (Chip8->opcode & 0x0F00) >> 8;

       uint8_t value = Chip8->opcode & 0x00FF;

       Chip8->v[vx] = value;
 
       break;
    } // this opcode implement the chip8 code which set register vx to value NN

    case 0x0004:
    {
      if(chip8->v[(chip8->opcode & 0x00F0) >> 4] > ( 0xFF - chip8->v[(chip8->opcodee & 0x0F00) >> 8]))
      {
        chip8->v[0xF] = 1; // carry 
      }
      else
      {
       chip8->v[0xF] = 0;
      }
       chip8->v[(chip8->opcode) >> 8] += chip8->v[(chip8->opcode & 0x00F0) >> 4];
       pc +=2;

      break;
    } // this opcode implemnet the chip8 opcode 0x8XY4 this opcode add the value of VY to VX .
    

    case 0x0033:  
    {
     chip8->memory[I]            = chip8->v[(chip8->opcode & 0x0F00) >> 8] /100;
     chip8->memory[I + 1]        = (chip8->v[(chip8->opcode & 0x0F00) >> 8] 10) %10;
     chip8->memory[I + 2]        = (chip8->v[(chip8->opcode & 0x0F00) >> 8] %100) % 10;
     pc +=2 ;
     break;
    } // this opcode implement the chip8 opcode 0xFX33 this stores the binary coded decimal representation of VX at addresses of I , I+1 , I+2 

    case 0xD000:
    {
     uint8_t x = chip8->v[(chip8->opcode & 0x0F00) >> 8];
     uint8_t y = chip8->v[(chip8->opcode & 0x00F0) >> 4];
     uint8_t height = chip8->opcode & 0x000F;
     
     v[0xF] = 0;
     for (int yline= 0; yline < height ; yline++)
       {
            uint8_t pixel = chip8->memory[chip8->I + yline];
            for(xline = 0; xline< 8 ; xline++)
            {
               if((pixel & (0x80 >> xline)) != 0 )
               {
                 int index = (x + xline) +((y + yline) * VIDEO_WIDTH);
                   if(chip8->gfx[index] ==1)
                   {
                      chip8->v[0xF] =1;
                   }
                   chip8->gfx[index] ^= 1;
               }
            }
        }
      
      drawFlag = true;
       break;
    } // this opcode implement the chip8 opcode 0xDXYN it drwas stripe
    
     default:
     {
        printf("unknown opcode : 0x%X\n" ,chip8->opcode);
        break;
     } 



  }

}
