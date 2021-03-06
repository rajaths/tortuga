;******************************************************************************
;*                                                                            *
;*  Project:     Tortuga bootloader                                           *
;*  Module:      iBL.s                                                        *
;*  Description: dsPic bootloader with autobaud detection                     *
;*               Read/Write through UART: PGM, EEPROM & Config registers      *
;*  Author:      Roger Juanpere/Neil Sikka                                    *
;*                                                                            *
;*  Revision: 1.0 (17-08-05): Initial version                                 *
;*            1.1 (01-02-06): Added support for >32K PGM devices              *
;*                                                                            *
;******************************************************************************
;*  ingenia-cat S.L. (c)   -   www.ingenia-cat.com                            *
;******************************************************************************

        .include "p30fxxxx.inc"

;******************************************************************************
; Configuration bits:
;******************************************************************************

        config __FOSC, CSW_FSCM_OFF & EC_PLL16    ;Turn off clock switching and
                                            ;fail-safe clock monitoring and
                                            ;use the External Clock as the
                                            ;system clock

        config __FWDT, WDT_OFF              ;Turn off Watchdog Timer

        config __FBORPOR, PBOR_ON & BORV_27 & PWRT_16 & MCLR_EN
                                            ;Set Brown-out Reset voltage and
                                            ;and set Power-up Timer to 16msecs

        config __FGS, CODE_PROT_OFF         ;Set Code Protection Off for the
                                            ;General Segment
/*
.equ RW       	_RC15
.equ TRIS_RW    _TRISC15

.equ RW_READ     0
.equ RW_WRITE    1
*/
;******************************************************************************
; Program Specific Constants (literals used in code)
;******************************************************************************
          .equ CRC, W4
          .equ ACK, 0x55
          .equ NACK, 0xFF
          .equ USER_ADDRESS, 0x0100
          .equ START_ADDRESS, 0x7D00                ; Relative to 0x0100

          .equ CFG_M, 0xF8
          .equ EE_M, 0x7F

          .equ C_READ, 0x01
          .equ C_WRITE, 0x02
          .equ C_VERSION, 0x03
          .equ C_USER, 0x0F
          .equ MAX_WORD_ROW, 48						; 96 byte packets(each word is 2 bytes)

          .equ MAJOR_VERSION, 0x01
          .equ MINOR_VERSION, 0x01

          .equ RW_PORT PORTC							; the R/W port
          .equ RW_TRIS TRISC							; the R/W TRIS register
          .equ RW_PIN 15

          .equ bootloaderBaseAddress 0xB18			;0d4096-0d256, if program mem(where were storing this) nonvolatile?
          											;User program space access is restricted to the lower 4M instruction
          											;word address range (0x000000 to 0x7FFFFE) p25 in datasheet

;******************************************************************************
; Global Declarations:
;******************************************************************************
          .global __reset          ;The label for the first line of code.
          .global recBuf

;******************************************************************************
;Uninitialized variables in X-space in data memory
;******************************************************************************
          		.section bss, xmemory
recBuf:   		.space 2 * MAX_WORD_ROW
imageBlockSize: .space 2				;is this the correct place/way to make a varible?

;******************************************************************************
;Code Section in Program Memory
;******************************************************************************
        .text                     ; Start of Code section
        .org #START_ADDRESS
__reset:
        MOV #__SP_init, W15       ; Initialize the Stack Pointer
        MOV #__SPLIM_init, W0     ; Initialize the Stack Pointer Limit Register
        MOV W0, SPLIM
        NOP                       ; Add NOP to follow SPLIM initialization






/*
 * ACK = D2			this section is wrong, use the deinitions below
 * RW  = E8
 */

/*
15 	 * Bus = D1 D0 E5-E0
16 	 * Req = C13
17 	 * Akn = C14
18 	 * RW  = C15
19 	 */


;TRIS = 1 ->input
;TRIS = 0 ->output

;D1(MSB)-D0,E5-E0(LSB)




/*i dont know how to do the IRQ stuff so im skipping that*/
		BSET TRIS_RW		;we want to set the RW pin to read to see if we need to read or write
		NOP
		BTSS RW_PORT, #RW_PIN		;is RW==1 ie WRITE mode? if so, skip next instruction
		call SendNACK
		call ReadBus 		;read the BUS so we can swap the nybbles
		SWAP W2				;swap the nybbles
;set timer
/*i dont know how to do the IRQ stuff so im skipping that*/
;if we get interrupt:
		BSET TRIS_RW		;we want to set the RW pin to read to see if we need to read or write
		NOP
		BTSS RW_PORT, #RW_PIN		;is RW==1 ie WRITE mode? if so, skip next instruction
		goto sendSwappedNybbles
		goto finished
afterSendingSwappedNybbles:
;set timer
/*i dont know how to do the IRQ stuff so im skipping that*/
;if we get IRQ:
		BSET TRIS_RW		;we want to set the RW pin to read to see if we need to read or write
		NOP
		BTSS RW_PORT, #RW_PIN		;is RW==1 ie WRITE mode? if so, skip next instruction
;send NACK- this call will not return-it will restart the chip
		CALL ReadBus		;get the Low byte of the image size
		MOV W2, imageBlockSize
;set timer
		CALL ReadBus		;get the High byte of the image size
		MOV W2, imageBlockSize+1
;set timer
;if we get IRQ
		BSET TRIS_RW		;we want to set the RW pin to read to see if we need to read or write
		NOP
		BTSC RW_PORT, #RW_PIN		;is RW==0 ie READ mode? if so, skip next instruction
;restart
		CALL assertImageSize
		MOV imageBlockSize, WREG
		ADD #0, WREG
		BRA Z, reset				;make sure imageBlockSize!=0
		BRA N, reset
;		ADD #0, imageBlockSize					;check if imageBlockSize==0
;		BRA Z, reset															avoid this code cuz its big
;		BRA N, reset
;		ADD #0, imageBlockSize+1				;check if imageBlockSize==0
;		BRA Z, reset
;		BRA N, reset
receiveLoop:							;TODO: START HERE THIS IS KIDNA WRONG-WE NEED TO ONLY WRITE 96 BYTES AT A TIME, add checksum support
		MOV imageBlockSize, WREG		;
		ADD #0, WREG					;
		BRA Z, afterReceive				;is our counter == 0?
		BRA N, afterReceive				;

		CALL ReadBus		;TODO:are we trying to use the ReadBus function or the ReceiveChar function?
							;probably Readbus because we arnet using the UART
		CALL writeBlock		;write the actual block to Flash
		DEC imageBlockSize	;imageBlockSize--
		GOTO receiveLoop
afterReceive:
;which PIN is the slave's request line?












ReadBus:					;read BUS->W2
		MOV 0x003F, W0		;we want to set the TRIS E5-E0 pins to read
		MOV W0, TRISE		;actually do it.
		MOV 0x0003, W0		;we want to set the TRIS D1-D0 pins to read
		MOV W0, TRISD		;actually do it.
		MOV LATE, W1		;read contents of Port E
		MOV LATD, W2		;read contents of Port D
		AND 0x003F, W1		;only store the lowest 6 bits in W1
		AND 0x0003, W2		;only store the lowest 2 bits in W2
		IOR W2, W1, W2		;now we have the full byte on the bus stored in W2
		return
;apparently we can write a whole byte at once by writing to a latch. the problem is that we need to write to parts of D and E

sendSwappedNybbles:			;write the contents of W2 into D[1:0],E[5:0]
							;LATD bits:		10
							;LATE bits:		  543210
											^^^^^^^^
							;register W2:	76543210
		BCLR RW_TRIS		;clear the RW pin so we can write the Nybble back
		MOV 0x000, W0		;we want to set the pins to write
		MOV W0, TRISE		;actually do it for E.
		MOV W0, TRISD		;actually do it for D.
		MOV W2, LATE									;will this write the lower bits only?
		AND 0xC0, W2		;we only want to preserve the 2 low bits of W2
		ASR W2, 6, W2		;W2=W2>>6 so the high bits of W2 are now bits 0/1
		MOV W2, LATD		;actually write to LATD
		goto afterSendingSwappedNybbles

assertImageSize:			;assert (bootloader_BaseAddress-96*imageBlockSize > 0) else restart
		MUL.UU imageBlockSize, 96, W8	;we might be stepping on some toes and squashing some DSP address
										;regs, but i dont care at this point cuz its a bootloader...
										;what if imageBlockSize is 2 bytes long?
		SUBR W8, bootloaderBaseAddress, W10
		BRA LT, reset






		/*      ; Uart init
        mov #0x8420, W0           ; W0 = 0x8420 -> 1000 0100 0010 0000b
        mov W0, U1MODE            ; Enable UART with Alternate IO, AutoBaud and 8N1
        clr U1STA

        ; Timer 3 init
        clr T3CON                 ; Stops any 16-bit Timer3 operation
        bclr IEC0, #T3IE          ; Disable Timer 3 interrupt
        setm PR3                  ; Set Timer 3 period to maximum value 0xFFFF
        mov #0x8000, W0           ; Start Timer 3 with 1:1 prescaler and clock source set to internal cycle
        mov W0, T3CON

        ; Input Capture init
        clr IC1CON                ; Turn off Input Capture 1 module
        bset IC1CON, #1           ; Input Capture Mode every risind edge
        bclr IFS0, #IC1IF         ; Clear Input Capture flag
        bclr IEC0, #IC1IE         ; Disable Input Capture interrupts

        ; Start Autobaud detection
        mov #0x0004, W0           ; W0 = 0x0004
        rcall WaitRising          ; Wait until the first Rising edge is detected
        clr TMR3                  ; Clear content of the Timer 3 timer register*/
/*ByteLoop:
        rcall WaitRising
        dec W0, W0                ; W0--
        bra NZ, ByteLoop          ; if W0 != 0 jump to ByteLoop
        bclr T3CON, #TON          ; Last Rising edge detected so Stop Timer 3
        mov TMR3, W0              ; W0 = TMR3
        add #0x40, W0             ; For rounding: +64 >> 7 is equal to +0.5
        asr W0, #7, W0            ; W0 = ((Tend - Tini + 64) / 128)
        dec W0, W0                ; W0--*/

/*        ; Uart re-init
        mov W0, U1BRG             ; U1BRG = W0 -> Configs UART with the detected baudrate
        bclr U1MODE, #ABAUD       ; Disable AutoBaud
        bset U1STA, #UTXEN        ; Enable transmition
        bra SendAck*/

StartFrame:
        btss U1STA, #URXDA        ; Wait until a character is received
        bra StartFrame
        mov U1RXREG, W0
        cp.b W0, #C_USER          ; Compare received Character with USER character
        btsc SR, #Z
        goto USER_ADDRESS
        cp.b W0, #C_READ          ; Compare received Character with READ character
        bra Z, ReadMemCmd
        cp.b W0, #C_WRITE         ; Compare received Character with WRITE character
        bra Z, WriteMemCmd
        cp.b W0, #C_VERSION       ; Compare received Character with VERSION character
        bra Z, VersionCmd
        bra SendNack              ; Unknown character -> Send NACK

VersionCmd:
        mov #MAJOR_VERSION, W0    ; Send Major Version
        mov W0, U1TXREG
        mov #MINOR_VERSION, W0    ; Send Minor Version
        mov W0, U1TXREG
        bra SendAck

ReadMemCmd:
        rcall ReceiveChar         ; Receive high byte of the address
        mov W0, TBLPAG            ; High address byte
        rcall ReceiveChar         ; Receive medium byte of the address
        swap W0
        rcall ReceiveChar         ; Receive low byte of the address

        tblrdh [W0], W1           ; Read high word to W1
        mov W1, U1TXREG           ; Send W1 low byte

        tblrdl [W0], W1           ; Read low word to W1
        swap W1
        mov W1, U1TXREG           ; Send W1 high byte
        swap W1
        mov W1, U1TXREG           ; Send W1 low byte
SendAck:
        mov #ACK, W0              ; Send an ACK character
        bra Send
SendNack:
        mov #NACK, W0             ; Send a KO character
Send:
        mov W0, U1TXREG
        bra StartFrame

WriteMemCmd:
        clr W4                    ; Reset W4 = Checkbyte
        rcall ReceiveChar         ; Receive high byte of the initial address
        mov W0, TBLPAG            ; For latch loading and programming
        mov W0, NVMADRU           ; For erase cycle - in program are written auto. from TBLPAG
        rcall ReceiveChar         ; Receive medium byte of the initial address
        mov.b WREG, NVMADR + 1
        rcall ReceiveChar         ; Receive low byte of the initial address
        mov.b WREG, NVMADR

        rcall ReceiveChar         ; Receive the number of bytes to be received
        mov W0, W3
        mov #recBuf, W2           ; W2 = recBuf
FillBufLoop:
        rcall ReceiveChar
        mov.b W0, [W2++]          ; Move received byte to recBuf
        dec W3, W3
        bra nz, FillBufLoop       ; Fill reception buffer

        cp0.b W4                  ; Check (INTEL HEX8 Checksum - Sum modulo 256)
        bra nz, SendNack          ; if Checkbyte != 0 jump to SendNack
        mov #recBuf, W2           ; W2 = recBuf
        mov NVMADR, W5            ; Use W5 as low word address

        mov #CFG_M, W0            ; Check if destination is Config Memory
        cp.b TBLPAG
        bra nz, noCFM

        mov #0x4008, W8           ; Assigns Write Config Row Code - Config Mem doesn't need to be erased
        mov #1, W3                ; Assigns Number of 16bits words per Row
        bra LoadLatch
noCFM:
        mov #EE_M, W0             ; Check if destination is EEPROM Memory
        cp.b TBLPAG
        bra NZ, noEEM
        mov #0x4075, W0           ; Assigns Erase EEPROM Row Code
        mov #0x4005, W8           ; Assigns Write EEPROM Row Code
        mov #32, W3               ; Assigns Number of 16bits word per Row
        bra StartWritingCycle     ; Erase and Write Memory
noEEM:
        mov #0x4071, W0           ; Assigns Erase PGM Row Code
        mov #0x4001, W8           ; Assigns Write PGM Row Code
        mov #64, W3               ; Assigns Number of 16bits word per Row (32instr - 64word16)

StartWritingCycle:
        rcall WriteKey            ; Erase selected Row
LoadLatch:
        tblwtl [W2++], [W5]       ; Load low word to latch
        dec W3, W3
        bra Z, EndLatch
        tblwth [W2++], [W5++]     ; Load high word to latch
        dec W3, W3                ; Repeat until whole row is loaded
        bra NZ, LoadLatch
EndLatch:
        mov W8, W0                ; Write selected Row
        rcall WriteKey
        bra SendAck               ; Send an ACK character


;******************************************************************************
;Procedures
;******************************************************************************
WaitRising:
        mov #0x5A, W2             ; W2 = 0x5A
MajorLRise:
        setm W1                   ; W1 = 0xFFFF
MinorLRise:
        btsc IFS0, #IC1IF         ; Rising edge detected?
        bra EndRising             ; Yes -> Jump to finish detection
        dec W1, W1                ; W1--
        bra NZ, MinorLRise        ; if W1 != 0 jump MinorLRise
        dec W2, W2                ; W2--
        bra NZ, MajorLRise        ; if W2 != 0 jump MajorLRise
        goto USER_ADDRESS         ; Timeout aprox. = 0x5A * 0xFFFF * 5 clocks -> Jump to user soft

EndRising:
        bclr IFS0, #IC1IF         ; Clear Interrupt Flag
        return

GetReadOrWrite:


;******************************************************************************
ReceiveChar:					  ;receive character into W0 from UART before timeout, checksum it.
        mov #0xFFFF, W10          ; W10 = 0xFFFF
MajorLChar:
        setm W11                  ; W11 = 0xFFFF
MinorLChar:
        btsc U1STA, #URXDA        ; Character received off UART?
        bra EndReceiveChar        ; Yes -> Jump to Finish reception
        dec W11, W11              ; W11--
        bra NZ, MinorLChar        ; if W11 != 0 jump MinorLChar
        dec W10, W10              ; W10-
        bra NZ, MajorLChar        ; if W10 != 0 jump MajorLChar
        MOV #__SP_init, W15       ; Initialize Stack Pointer
        bra SendNack              ; Timeout aprox. = 0xFFFF * 0xFFFF * 5 clocks -> Jump to Send Nack
EndReceiveChar:
        mov.b U1RXREG, WREG       ; W0 = U1RXREG
        add.b W4, W0, W4          ; Checkbyte += W0 -> Performs a Sum modulo 256 checksum (INTEL HEX8)
        return

;******************************************************************************
WriteKey:
        mov W0, NVMCON
        mov #0x55, W0
        mov W0, NVMKEY
        mov #0xAA, W0
        mov W0, NVMKEY
        bset NVMCON, #WR          ; Start Writing
        nop
        nop
WaitWriting:
        btsc NVMCON, #WR          ; WR or WREN - Wait until operation is finished
        bra WaitWriting
        return

;--------End of All Code Sections ---------------------------------------------

.end                              ; End of program code in this file
