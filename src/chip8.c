void chip8_init(Chip8 *chip8){

    memset(chip8,0,sizeof(Chip8)); //this sets the entire memory to 0
    /* we can use memset to reset the memory to 0 or 
       
       for(int i = 0; i < 2048; ++i)
		       gfx[i] = 0;

       we can use like this for all other variables in the struct which we are using as a emulator information*/

    chip8->pc = 0x200; // 0x200-0xFFF - Program ROM and work RAM

    chip8->I = 0;
    chip8->opcode = 0;

}

void chip8_load_application(Chip8 *chip8 , const char *filename){
    chip8_init(Chip8);
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
    char * buffer = (char*)malloc(sizeof(char) * size);
    if (buffer == NULL){
        fputs("MEMORY ERROR",stderr);
        exit(1);
    }

    //copy the whole file into the buffer
    size_t result = fread (buffer,1,size,file);
    if(result != size){
        fputs("READING ERROR",stderr);
        exit(1);
    }

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
  chip8->opcode = chip8->memory[pc] << 8 | memory[pc + 1]; 
  /* as one opcode is two byte long we will need to fetch two
   * succesive byte and merge them to get an actual opcode .*/

  // decode opcode
  switch (chip8->opcode & 0xF000)
  {
      case 0x1000:
      {
       uint16_t address = chip8->opcode & 0x0FFF;
        chip8->pc = address;
        break;
      } /* this opcode implement the chip8 1NNN instruction which means jump to address NNN */
      
      case 0x2000:
      {
         chip8->stack[sp] = pc;
         ++sp;
         chip8->pc = chip8->opcode & 0x0FFF;
         break;
      } // this opcode calls the sub routine at address NNN

      case 0x6000;
      {
       



      }
      








  }
  


}
