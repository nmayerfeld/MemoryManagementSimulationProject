#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

int power;
void **pagetable;
void **diskTable; //will store location on "disk"
char *locationTable; //will store a 0 or 1 to indicate if this virtual pg is in physical memory or only on disk.  Disclaimer- normally this is stored in the first bit of mem address but bc dealing with string manipulation for addresses as representation- much simpler to just store it separately than convert to int add a bit and convert back to string each time
long *FIFOTable; //store order of indices to be removed when physical memory is full and need to boot a p to disk
char *modifiedTable; //store 1 or 0 to indicate if modified since moved to physical memory and needs to be copied back onto disk
int ppmc;
int currentPhysicalFull=0;
int nextIndexForFIFOTable=0;
int nextToBoot=0;
long pageSize;
char *myPointer;
char *myPointerForBoot;
char binaryArray[65];
int pageTableSize;
int sizeOfAddresses;
int sizeForBinaryArrays;
int ex=0;
char *bootArray;
void computeSizeForBinaryArrays(int pagenumber){
    int ppp=0;
    int result=1;
    while(result+result-1<pagenumber){
        ppp++;
        result*=2;
    }
    ppp++;
    sizeForBinaryArrays=ppp+power+1;
}
long powe(int x, int y){
    long currentResult=1;
    for(int i=1;i<=y;i++){
        currentResult*=x;
    }
    return currentResult;
}
int gimmeSize(char *arr){
    int size=0;
    while(*arr!='\0'){
        size++;
        arr++;
    }
    return size;
}
void takesCharArrayAndSetsUPBinaryArray(char *address){
    int place=0;
    unsigned long number=0;
    for(int i=sizeOfAddresses-1;i>1;i--){
        char current=address[i];
        if(current=='A'){
            number+=10*(powe(16,place));
        }
        else if(current=='B'){
            number+=11*(powe(16,place));
        }
        else if(current=='C'){
            number+=12*(powe(16,place));
        }
        else if(current=='D'){
            number+=13*(powe(16,place));
        }
        else if(current=='E'){
            number+=14*(powe(16,place));
        }
        else if(current=='F'){
            number+=15*(powe(16,place));
        }
        else if(isdigit(current)){
            number+=(current-48)*(powe(16,place));
        }
        else{
            fprintf(stderr,"\nincorrect value inputted");
            exit(3);
        }
        place++;
    }
    //find out how many spots needed in binary array
    int cur=1;
    int po=0;
    while(cur<number){
        po++;
        cur*=2;
    }
    po++;
    int p=po-1;
    for(int i=64-po;i<64;i++){
        if(number>=powe(2,p)){
            binaryArray[i]=49;
            number-=(powe(2,p));
        }
        else{
            binaryArray[i]=48;
        }
        p--;
    }
    binaryArray[64]='\0';
    for(int i=0;i<64-po;i++){
        binaryArray[i]=48;
    }
}
void parse(char *buffer, char **args){
    int argc;
    argc=0;
    char *firstSpace;
    char *firstNewLine= strchr(buffer,'\n');
    *firstNewLine=' '; //sets the char right after the end of the input to a ' ' so the strchr in the while loop will include the last argument
    while((firstSpace=strchr(buffer,' '))){ //sets firstSpace to point at the char after the current argument
        args[argc++]=buffer;  //sets the next index of args to buffer
        *firstSpace='\0'; //sets the buffer[index after current arg] to null, so args[current] will contain the argument and then a null termination
        buffer=firstSpace+1; //shifts the buffer pointer one index past the null, to the first char of the next argument (or null if all arguments are taken care of)
    }
    args[argc]=NULL; //making argv null terminated;
}
long convertCharArrayAddressToVirtualPageNumber(char *address){
    takesCharArrayAndSetsUPBinaryArray(address);
    //last power bytes are the offset, everything before that is the page number
    int place=0;
    int num=0;
    for(int i=63-power;i>=0;i--){
        char current=binaryArray[i];
        if(current=='1'){
            num+= powe(2,place);
        }
        place++;
    }
    return num;
}
long calculateOffsetFromAddress(char *address){
    int startIndex=64-power;
    int place=0;
    int numb=0;
    for(int i=63;i>=startIndex;i--){
        char current=binaryArray[i];
        if(current=='1'){
            numb+= powe(2,place);
        }
        place++;
    }
    return numb;
}
void convertPMLocationBackToHexString(char *location){
    char result[sizeOfAddresses+1];
    result[0]=48;
    result[1]='x';
    result[sizeOfAddresses]='\0';
    int place=15;
    long numLeft=location;
    for(int i=2;i<sizeOfAddresses;i++){
        long numToPut=numLeft/(powe(16,place));
        if(numToPut>0){
            if(numToPut==10){
                result[i]='A';
            }
            else if(numToPut==11){
                result[i]='B';
            }
            else if(numToPut==12){
                result[i]='C';
            }
            else if(numToPut==13){
                result[i]='D';
            }
            else if(numToPut==14){
                result[i]='E';
            }
            else if(numToPut==15){
                result[i]='F';
            }
            else if(numToPut<10){
                result[i]=numToPut+48; //do i need to add 48 or just cast to char
            }
            else{
                fprintf(stderr,"\nunknown error encountered with num to put");
                exit(3);
            }
            numLeft=numLeft-(numToPut* powe(16,place));
        }
        else{
            result[i]=48;
        }
        place--;
    }
    myPointer=result;
}
void bootPage(){
    long pageToBoot=FIFOTable[nextToBoot];
    long ptb=pageToBoot;
    char binaryArrayForBoot[65];
    int ppppp = 63 - power;
    for (int i = 0; i <= 63 - power; i++) {
        long toSubtract = powe(2, ppppp);
        ppppp--;
        if (ptb >= toSubtract) {
            ptb -= toSubtract;
            binaryArrayForBoot[i] = 49;
        } else { binaryArrayForBoot[i] = 48; }
    }
    for (int i = 64 - power; i < 65; i++) {
        binaryArrayForBoot[i] = 48;
    }
    binaryArrayForBoot[64] = '\0';
    char vp[sizeOfAddresses+1];
    vp[0]=48;
    vp[1]='x';
    long sumForVP=0;
    int exp=power;
    for(int i=63-power;i>=0;i--){
        char cu=binaryArrayForBoot[i];
        if(cu=='1'){
            sumForVP+= powe(2,exp);
        }
        exp++;
    }
    int ex=15;
    for(int i=2;i<sizeOfAddresses;i++){
        long ntp=sumForVP/(powe(16,ex));
        if(ntp>0){
            if(ntp==10){
                vp[i]='A';
            }
            else if(ntp==11){
                vp[i]='B';
            }
            else if(ntp==12){
                vp[i]='C';
            }
            else if(ntp==13){
                vp[i]='D';
            }
            else if(ntp==14){
                vp[i]='E';
            }
            else if(ntp==15){
                vp[i]='F';
            }
            else if(ntp<10){
                vp[i]=ntp+48; //do i need to add 48 or just cast to char
            }
            else{
                fprintf(stderr,"\nunknown error encountered with num to put");
                exit(3);
            }
            sumForVP=sumForVP-(ntp* powe(16,ex));
        }
        else{
            vp[i]=48;
        }
        ex--;
    }
    vp[sizeOfAddresses]='\0';
    if(modifiedTable[pageToBoot]=='1'){
        void *diskAddress=diskTable[pageToBoot];
        void *physicalMemAddress=pagetable[pageToBoot];
        memcpy(diskAddress,physicalMemAddress,pageSize);
        modifiedTable[pageToBoot]='0';
        locationTable[pageToBoot]='0';
        currentPhysicalFull--;
        unsigned long physicalLocation=pagetable[pageToBoot];
        fprintf(stdout,"physical page at 0x%016lX, which corresponds to virtual page %s, is evicted and dirty. Copied to disc ",physicalLocation,vp);
        unsigned long diskLocation=diskTable[pageToBoot];
        fprintf(stdout,"at 0x%016lX and removed from physical memory\n",diskLocation);
        free(pagetable[pageToBoot]);
        pagetable[pageToBoot]=diskTable[pageToBoot];
        bootArray='\0';
    }
    else{
        locationTable[pageToBoot]='0';
        currentPhysicalFull--;
        unsigned long physicalLocation=pagetable[pageToBoot];
        fprintf(stdout,"physical page at 0x%016lX, which corresponds to virtual page %s, is evicted and not dirty. Copied to disc ",physicalLocation,vp);
        unsigned long diskLocation=diskTable[pageToBoot];
        fprintf(stdout,"at 0x%016lX and removed from physical memory\n",diskLocation);
        free(pagetable[pageToBoot]);
        pagetable[pageToBoot]=diskTable[pageToBoot];
    }
    if(nextToBoot==ppmc-1){
        nextToBoot=0;
    }
    else{nextToBoot++;}
}

void readbyte(char **args){
    long l=convertCharArrayAddressToVirtualPageNumber(args[1]);
    if(l>pageTableSize-1){
        fprintf(stderr,"\nreadbyte: segmentation fault");
    }
    else if(pagetable[l]==NULL){
        if(currentPhysicalFull==ppmc){
            bootPage();
        }
        void *loc;
        size_t alignment;
        size_t size=pageSize;
        if(pageSize<8){
            alignment=8;
        }
        else{
            alignment=pageSize;
        }
        int errorChecker=posix_memalign(&loc,alignment,size);
        if(errorChecker!=0){
            fprintf(stderr,"\nposix_memalign failed. aborting");
            exit(2);
        }
        pagetable[l]=loc;
        FIFOTable[nextIndexForFIFOTable]=l;
        locationTable[l]='1';
        nextIndexForFIFOTable++;
        if(nextIndexForFIFOTable==ppmc){
            nextIndexForFIFOTable=0;
        }
        currentPhysicalFull++;
        int s=gimmeSize(args[1]);
        char vp[sizeOfAddresses+1];
        vp[0]=48;
        vp[1]='x';
        long sumForVP=0;
        int exp=power;
        for(int i=63-power;i>=0;i--){
            char cu=binaryArray[i];
            if(cu=='1'){
                sumForVP+= powe(2,exp);
            }
            exp++;
        }
        int ex=15;
        for(int i=2;i<sizeOfAddresses;i++){
            long ntp=sumForVP/(powe(16,ex));
            if(ntp>0){
                if(ntp==10){
                    vp[i]='A';
                }
                else if(ntp==11){
                    vp[i]='B';
                }
                else if(ntp==12){
                    vp[i]='C';
                }
                else if(ntp==13){
                    vp[i]='D';
                }
                else if(ntp==14){
                    vp[i]='E';
                }
                else if(ntp==15){
                    vp[i]='F';
                }
                else if(ntp<10){
                    vp[i]=ntp+48; //do i need to add 48 or just cast to char
                }
                else{
                    fprintf(stderr,"\nunknown error encountered with num to put");
                    exit(3);
                }
                sumForVP=sumForVP-(ntp* powe(16,ex));
            }
            else{
                vp[i]=48;
            }
            ex--;
        }
        vp[sizeOfAddresses]='\0';
        convertPMLocationBackToHexString(loc);
        fprintf(stdout,"physical page at %s mapped to virtual page at %s\n",myPointer,vp);
        long offset= calculateOffsetFromAddress(args[1]);
        unsigned char *location=pagetable[l]+offset;
        convertPMLocationBackToHexString(location);
        fprintf(stdout,"readbyte: VM location %s, which is PM location %s, contains value 0x%02hhX",args[1],myPointer,*location);
    }
    else{
        if(locationTable[l]=='1'){
            long offset= calculateOffsetFromAddress(args[1]);
            unsigned char *location=pagetable[l]+offset;
            char **pmLocation;
            convertPMLocationBackToHexString(location);
            fprintf(stdout,"readbyte: VM location %s, which is PM location %s, contains value 0x%02hhX",args[1],myPointer,*location);
        }
        else{
            //on disk so get address of stuff on disk
            if(currentPhysicalFull==ppmc){
                bootPage();
            }
            void *diskAddress=diskTable[l];
            void *loc;
            size_t alignment;
            size_t size=pageSize;
            if(pageSize<8){
                alignment=8;
            }
            else{
                alignment=pageSize;
            }
            int errorChecker=posix_memalign(&loc,alignment,size);
            if(errorChecker!=0){
                fprintf(stderr,"\nposix_memalign failed. aborting");
                exit(2);
            }
            pagetable[l]=loc;
            currentPhysicalFull++;
            FIFOTable[nextIndexForFIFOTable]=l;
            locationTable[l]='1';
            nextIndexForFIFOTable++;
            if(nextIndexForFIFOTable==ppmc){
                nextIndexForFIFOTable=0;
            }
            int s=gimmeSize(args[1]);
            char vp[sizeOfAddresses+1];
            vp[0]=48;
            vp[1]='x';
            long sumForVP=0;
            int exp=power;
            for(int i=63-power;i>=0;i--){
                char cu=binaryArray[i];
                if(cu=='1'){
                    sumForVP+= powe(2,exp);
                }
                exp++;
            }
            int ex=15;
            for(int i=2;i<sizeOfAddresses;i++){
                long ntp=sumForVP/(powe(16,ex));
                if(ntp>0){
                    if(ntp==10){
                        vp[i]='A';
                    }
                    else if(ntp==11){
                        vp[i]='B';
                    }
                    else if(ntp==12){
                        vp[i]='C';
                    }
                    else if(ntp==13){
                        vp[i]='D';
                    }
                    else if(ntp==14){
                        vp[i]='E';
                    }
                    else if(ntp==15){
                        vp[i]='F';
                    }
                    else if(ntp<10){
                        vp[i]=ntp+48; //do i need to add 48 or just cast to char
                    }
                    else{
                        fprintf(stderr,"\nunknown error encountered with num to put");
                        exit(3);
                    }
                    sumForVP=sumForVP-(ntp* powe(16,ex));
                }
                else{
                    vp[i]=48;
                }
                ex--;
            }
            vp[sizeOfAddresses]='\0';
            convertPMLocationBackToHexString(loc);
            fprintf(stdout,"physical page at %s mapped to virtual page at %s\n",myPointer,vp);
            memcpy(loc,diskAddress,pageSize); //copy memory;
            long offset= calculateOffsetFromAddress(args[1]);
            unsigned char *location=pagetable[l]+offset;
            char **pmLocation;
            convertPMLocationBackToHexString(location);
            fprintf(stdout,"readbyte: VM location %s, which is PM location %s, contains value 0x%02hhX",args[1],myPointer,*location);
        }
    }
}
unsigned char convertValToUnsignedCharForWriting(char *valToWrite){
    int currentVal;
    char ch=valToWrite[3];
    if(ch=='A'){currentVal=10;}
    else if(ch=='B'){currentVal=11;}
    else if (ch=='C'){currentVal=12;}
    else if(ch=='D'){currentVal=13;}
    else if(ch=='E'){currentVal=14;}
    else if(ch=='F'){currentVal=15;}
    else if(isdigit(ch)){currentVal=ch-48;}
    int result=currentVal;
    ch=valToWrite[2];
    if(ch=='A'){currentVal=10;}
    else if(ch=='B'){currentVal=11;}
    else if (ch=='C'){currentVal=12;}
    else if(ch=='D'){currentVal=13;}
    else if(ch=='E'){currentVal=14;}
    else if(ch=='F'){currentVal=15;}
    else if(isdigit(ch)){currentVal=ch-48;}
    result+=16*currentVal;
    if(result>255){
        fprintf(stderr,"major issue as val to write is greater than 255");
        exit(4);
    }
    unsigned char c=result;
    return c;
}
void writebyte(char **args){
    long l=convertCharArrayAddressToVirtualPageNumber(args[1]);
    modifiedTable[l]='1';
    if(l>pageTableSize-1){
        fprintf(stderr,"\nwritebyte: segmentation fault");
    }
    else if(pagetable[l]==NULL){
        if(currentPhysicalFull==ppmc){
            bootPage();
        }
        void *loc;
        size_t alignment;
        size_t size=pageSize;
        if(pageSize<8){
            alignment=8;
        }
        else{
            alignment=pageSize;
        }
        int errorChecker=posix_memalign(&loc,alignment,size);
        if(errorChecker!=0){
            fprintf(stderr,"\nposix_memalign failed. aborting");
            exit(2);
        }
        pagetable[l]=loc;
        FIFOTable[nextIndexForFIFOTable]=l;
        locationTable[l]='1';
        nextIndexForFIFOTable++;
        if(nextIndexForFIFOTable==ppmc){
            nextIndexForFIFOTable=0;
        }
        currentPhysicalFull++;
        int s=gimmeSize(args[1]);
        char vp[sizeOfAddresses+1];
        vp[0]=48;
        vp[1]='x';
        long sumForVP=0;
        int exp=power;
        for(int i=63-power;i>=0;i--){
            char cu=binaryArray[i];
            if(cu=='1'){
                sumForVP+= powe(2,exp);
            }
            exp++;
        }
        int ex=15;
        for(int i=2;i<sizeOfAddresses;i++){
            long ntp=sumForVP/(powe(16,ex));
            if(ntp>0){
                if(ntp==10){
                    vp[i]='A';
                }
                else if(ntp==11){
                    vp[i]='B';
                }
                else if(ntp==12){
                    vp[i]='C';
                }
                else if(ntp==13){
                    vp[i]='D';
                }
                else if(ntp==14){
                    vp[i]='E';
                }
                else if(ntp==15){
                    vp[i]='F';
                }
                else if(ntp<10){
                    vp[i]=ntp+48; //do i need to add 48 or just cast to char
                }
                else{
                    fprintf(stderr,"\nunknown error encountered with num to put");
                    exit(3);
                }
                sumForVP=sumForVP-(ntp* powe(16,ex));
            }
            else{
                vp[i]=48;
            }
            ex--;
        }
        vp[sizeOfAddresses]='\0';
        convertPMLocationBackToHexString(loc);
        fprintf(stdout,"physical page at %s mapped to virtual page at %s\n",myPointer,vp);
        long offset= calculateOffsetFromAddress(args[1]);
        unsigned char *location=pagetable[l]+offset;
        unsigned char toWrite=convertValToUnsignedCharForWriting(args[2]);
        *location=toWrite;
        convertPMLocationBackToHexString(location);
        fprintf(stdout,"writebyte: VM location %s, which is PM location %s, now contains value %s",args[1],myPointer,args[2]);
    }
    else{
        if(locationTable[l]=='1'){
            long offset= calculateOffsetFromAddress(args[1]);
            unsigned char *location=pagetable[l]+offset;
            unsigned char toWrite=convertValToUnsignedCharForWriting(args[2]);
            *location=toWrite;
            convertPMLocationBackToHexString(location);
            fprintf(stdout,"writebyte: VM location %s, which is PM location %s, now contains value %s",args[1],myPointer,args[2]);
        }
        else {
            //on disk so get address of stuff on disk
            if (currentPhysicalFull == ppmc) {
                bootPage();
            }
            void *diskAddress = diskTable[l];
            void *loc;
            size_t alignment;
            size_t size = pageSize;
            if (pageSize < 8) {
                alignment = 8;
            } else {
                alignment = pageSize;
            }
            int errorChecker = posix_memalign(&loc, alignment, size);
            if (errorChecker != 0) {
                fprintf(stderr, "\nposix_memalign failed. aborting");
                exit(2);
            }
            pagetable[l] = loc;
            currentPhysicalFull++;
            FIFOTable[nextIndexForFIFOTable] = l;
            locationTable[l] = '1';
            nextIndexForFIFOTable++;
            if(nextIndexForFIFOTable==ppmc){
                nextIndexForFIFOTable=0;
            }
            int s = gimmeSize(args[1]);
            char vp[sizeOfAddresses + 1];
            vp[0] = 48;
            vp[1] = 'x';
            long sumForVP = 0;
            int exp = power;
            for (int i = 63 - power; i >= 0; i--) {
                char cu = binaryArray[i];
                if (cu == '1') {
                    sumForVP += powe(2, exp);
                }
                exp++;
            }
            int ex = 15;
            for (int i = 2; i < sizeOfAddresses; i++) {
                long ntp = sumForVP / (powe(16, ex));
                if (ntp > 0) {
                    if (ntp == 10) {
                        vp[i] = 'A';
                    } else if (ntp == 11) {
                        vp[i] = 'B';
                    } else if (ntp == 12) {
                        vp[i] = 'C';
                    } else if (ntp == 13) {
                        vp[i] = 'D';
                    } else if (ntp == 14) {
                        vp[i] = 'E';
                    } else if (ntp == 15) {
                        vp[i] = 'F';
                    } else if (ntp < 10) {
                        vp[i] = ntp + 48; //do i need to add 48 or just cast to char
                    } else {
                        fprintf(stderr, "\nunknown error encountered with num to put");
                        exit(3);
                    }
                    sumForVP = sumForVP - (ntp * powe(16, ex));
                } else {
                    vp[i] = 48;
                }
                ex--;
            }
            vp[sizeOfAddresses] = '\0';
            convertPMLocationBackToHexString(loc);
            fprintf(stdout, "physical page at %s mapped to virtual page at %s\n", myPointer, vp);
            memcpy(loc, diskAddress, pageSize); //copy memory;
            long offset = calculateOffsetFromAddress(args[1]);
            unsigned char *location = pagetable[l] + offset;
            unsigned char toWrite = convertValToUnsignedCharForWriting(args[2]);
            *location = toWrite;
            convertPMLocationBackToHexString(location);
            fprintf(stdout, "writebyte: VM location %s, which is PM location %s, now contains value %s", args[1],
                    myPointer, args[2]);
        }
    }
}
void executeCommand(char *cmdline){
    char *argv[3];
    char buffer[1000];
    strcpy(buffer,cmdline);
    parse(buffer,argv);
    if(strcmp(argv[0],"exit")!=0){sizeOfAddresses=gimmeSize(argv[1]);}
    if(strcmp(argv[0],"readbyte")==0){
        readbyte(argv);
    }
    else if(strcmp(argv[0],"writebyte")==0){
        writebyte(argv);
    }
    else if(strcmp(argv[0],"exit")==0){
        ex=1;
        for(int i=0;i<pageTableSize;i++){
            if(locationTable[i]=='1'){
                free(pagetable[i]);
            }
            free(diskTable[i]);
        }
    }
    else{
        fprintf(stderr,"invalid command entered");
        exit(4);
    }
}
int main(int argc, char **argv){
    if(argc!=4){
        fprintf(stderr,"incorrect num arguments entered. you entered %d. closing program",argc);
        return 1;
    }
    pageSize= atoi(argv[1]);
    int counter=0;
    int currently=1;
    while(currently+currently-1<pageSize){
        currently*=2;
        counter++;
    }
    if(currently!=pageSize){
        fprintf(stderr,"error: pagesize must be power of 2\n");
        return 1;
    }
    power=counter;

    computeSizeForBinaryArrays(atoi(argv[2]));
    //routine to align memory;

    //routine to allow for commands see compOrg txtbook p754
    char cmdline[1000];
    for(int i=0;i<1000;i++){
        cmdline[i]='\0';
    }
    pageTableSize=atoi(argv[2]);
    ppmc=atoi(argv[3]);
    void *pt[pageTableSize];
    void *dt[pageTableSize];
    char lt[pageTableSize];
    long ft[atoi(argv[3])];
    char mt[pageTableSize];
    for(int i=0;i<pageTableSize;i++){
        pt[i]='\0';
        lt[i]='0';
        mt[i]='0';
        void *loc;
        size_t alignment;
        size_t size=pageSize;
        if(pageSize<8){
            alignment=8;
        }
        else{
            alignment=pageSize;
        }
        int errorChecker=posix_memalign(&loc,alignment,size);
        if(errorChecker!=0){
            fprintf(stderr,"posix_memalign failed. aborting");
            exit(2);
        }
        dt[i]=loc;
    }
    for(int i=0;i<atoi(argv[3]);i++){
        ft[i]='\0';
    }
    pagetable=pt;
    diskTable=dt;
    locationTable=lt;
    FIFOTable=ft;
    modifiedTable=mt;
    while(1){
        printf("> ");
        fgets(cmdline,10000,stdin);
        if(feof(stdin))
            exit(0);
        executeCommand(cmdline);
        if(ex){return 0;}
        printf("\n");
    }
    return 0;
}