#include <SDL2/SDL.h>
#include <stdlib.h>
#include <stdio.h>

unsigned short pc;            //the program counter
unsigned short stack[24];
unsigned short I;
unsigned char V[16];          //the reg file: V0, V1, ... VF
unsigned char memory[4096];   //0x200, 0x201, ... 0xFFF hold instruction memory and data memory 
unsigned char screen[64][32]; //array for screen pixels. 1 is on 0 is off
unsigned char key[16];        //index to find state of hex key from keypad
unsigned char delay_timer;
unsigned char sound_timer;
unsigned char sp;             //the stack pointer



//zeroes out the display
void clear_screen()
{
  int i, j;
  for(i = 0; i < 64; i++)
  {
    for(j = 0; j < 32; j++)
      screen[i][j] = 0;
  }
}





//emulates one instruction
void emulate_instruction()
{
  unsigned short opcode;
  unsigned char instruction[4]; //each index holds four bits of the opcode where instruction[0] is the four msb's

  //instrction fetch stage - instructions are 2 bytes wide and stored in big endian
  opcode = memory[pc] << 8 | memory[pc + 1];

  //instruction decode stage. breaking up the opcode
  instruction[0] = (opcode >> 3 * 4) & 0xF;
  instruction[1] = (opcode >> 2 * 4) & 0xF;
  instruction[2] = (opcode >> 1 * 4) & 0xF;
  instruction[3] = (opcode >> 0 * 4) & 0xF;


  //instruction execute stage
  switch(instruction[0])
  {
    case 0:
      //clears the screen by zeroing out the screen[][] array
      if(instruction[1] == 0 && instruction[2] == 0xE && instruction[3] == 0)
        clear_screen();
      
      //returns from a subroutine
      if(instruction[1] == 0 && instruction[2] == 0xE && instruction[3] == 0xE)
      {
        //set pc to return address and pop from stack
        pc = stack[sp];
        sp--;
      }
      break;


    //unconditional jump/goto NNN for opcode = 1NNN
    case 1:
      pc = opcode & 0x0FFF;
      break;


    //calling subroutine at an adress NNN for opcode 2NNN
    case 2:
      sp++;
      stack[sp] = pc + 2;
      pc = opcode & 0x0FFF;
      break;


    //if Vx == NN skip next instruction for opcode = 3XNN
    case 30
      if(V[instruction[1]] == (opcode & 0x00FF))
        pc += 2; //should skip next instruction because we += 2 again at the end
      break;


    //same as case 3 but checking for inequality
    case 4:
      if(V[instruction[1]] != (opcode & 0x00FF))
        pc += 2;
      break;


    //skips next instruction if registers Vx == Vy for opcode = 5XY0
    case 5:
      if(V[instruction[1]] == V[instruction[2]])
        pc += 2;
      break;


    //sets Vx to NN for opcode = 6XNN
    case 6:
      V[instruction[1]] = opcode & 0x00FF;
      break;


    //add NN to Vx for opcode 7XNN but don't change carry flag
    case 7:
      V[instruction[1]] += opcode & 0x0FF;                                 //might want to do some fancy bounds checking, but also might naturally do what we want since its a char
      break;

    case 8:
      switch(instruction[3])
      {
        //sets Vx = Vy
        case 0:
          V[instruction[1]] = V[instruction[2]];
          break;

        //bitwise or's Vx and Vy and sets Vx to the result
        case 1:
          V[instruction[1]] |= V[instruction[2]];
          break;

        //bitwise and's Vx and Vy and sets Vx to the result
        case 2:
          V[instruction[1]] &= V[instruction[2]];
          break;

        //xor's Vx and Vy and sets Vx to the result
        case 3:
          V[instruction[1]] ^= V[instruction[2]];
          break;

        //adds Vy to Vx and sets VF to 1 on carry
        case 4:
          //checking for a carry
          if(V[instruction[1]] + V[instruction[2]] > 0xFF)
            V[0xF] = 1;
          else
            V[0xF] = 0;
          
          //performing the instruction
          V[instruction[1]] = (V[instruction[1]] + V[instruction[2]]) & 0xFF;
          break;

        //Vx -= Vy and set VF to 0 if borrow, 1 if no borrow
        case 5:
          //checking for borrow
          if(V[instruction[1]] < V[instruction[2]])
            V[0xF] = 0;
          else
            V[0xF] = 1;

          //performing the subtraction
          V[instruction[1]] -= V[instruction[2]];
          break;

        //shifts Vx to the right by one and stores lsb in VF
        case 6:
          V[0xF] = V[instruction[1]] & 0x1;
          V[instruction[1]] >>= 1;
          break;

        //Vx = Vy - Vx setting VF when there's no borrow
        case 7:
          if(V[instruction[1]] < V[instruction[2]])
            V[0xF] = 1;
          else
            V[0xF] = 0;

          V[instruction[1]] = V[instruction[2]] - V[instruction[1]];
          break;

        //stores msb of Vx in VF and shifts to the left one
        case 0xE:
          V[0xF] = V[instruction[1]] & 0x80;
          V[instruction[1]] << 1;
          break;

        default:
          printf("ILLEGAL INSTRUCTION: %x", opcode);
          break;

      }
      break;


    //skips next instruction in Vx != Vy
    case 9:
      if(V[instruction[1]] != V[instruction[2]])
        pc += 2;
      break;
    

    //sets I to NNN for opcode = ANNN
    case 0xA:
      I = opcode & 0x0FFF;
      break;


    //jumps to NNN + V0 with opcode = BNNN
    case 0xB:
      pc = V[0] + (opcode & 0x0FFF);
      break;


    //set Vx to the bitwise and of a random number and NN for opcode = CXNN
    case 0xC:
      //random number in range 0-255
      int r = rand(256);
      V[instruction[1]] = r & (opcode & 0x00FF);
      break;


    //draw a sprite to the screen
    case 0xD:
      draw();                                                               //not yet implemented. make sure to return a 0 or 1 and use that to set VF for collision detection
      break;


    case 0xE:
      if(instruction[2] == 9 && instruction[3] == 0xE)
        //check if key stored in Vx is pressed
        //if it is then skip next instruction
      if(instruction[2] == 0xA && instrucion[3] == 1)
        //if key stored in Vx isnt pressed then skip next instruction
      break;


    case 0xF:
      if((opcode & 0xFF) == 0x07)
        V[instruction[1]] = delay_timer;

      if((opcode & 0xFF) == 0x0A)
        //WAIT for a key to be pressed then store it in Vx

      if((opcode & 0xFF) == 0x15)
        delay_timer = V[instruction[1]];

      if((opcode & 0xFF) == 0x18)
        sound_timer = V[instruction[1]];

      if((opcode & 0xFF) == 0x1E)
        if(I + V[instruction[1]] > 0xFFF)
          V[0xF] = 1;
        else
          V[0xF] = 0
        I += V[instruction[1]]

      if((opcode & 0xFF) == 0x29)
        // uhhh not sure about this one
      if((opcode & 0xFF) == 0x)
      if((opcode & 0xFF) == 0x)
      if((opcode & 0xFF) == 0x)
      break;

    default:
      printf("ILLEGAL INSTRUCTION: %x", opcode);
      break;
  } 



  pc += 2;
  //then update timers
  //probably also display
}

//mips pc += 4, for chip-8 pc += 2
int main(int argc, char **argv)
{
  
  return 0;
}
