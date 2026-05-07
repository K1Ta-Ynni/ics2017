#include "cpu/exec.h"

make_EHelper(mov) {
  operand_write(id_dest, &id_src->val);
  print_asm_template2(mov);
}

make_EHelper(push) {
  rtl_push(&id_dest->val);

  print_asm_template1(push);
}

make_EHelper(pop) {
  rtl_pop(&t2);
  operand_write(id_dest, &t2);

  print_asm_template1(pop);
}

make_EHelper(pusha) {
  if (decoding.is_operand_size_16) {
    rtl_lr_w(&t0, R_AX); rtl_push(&t0);
    rtl_lr_w(&t0, R_CX); rtl_push(&t0);
    rtl_lr_w(&t0, R_DX); rtl_push(&t0);
    rtl_lr_w(&t0, R_BX); rtl_push(&t0);
    rtl_lr_w(&t0, R_SP); rtl_push(&t0);
    rtl_lr_w(&t0, R_BP); rtl_push(&t0);
    rtl_lr_w(&t0, R_SI); rtl_push(&t0);
    rtl_lr_w(&t0, R_DI); rtl_push(&t0);
  }
  else {
    rtl_lr_l(&t0, R_EAX); rtl_push(&t0);
    rtl_lr_l(&t0, R_ECX); rtl_push(&t0);
    rtl_lr_l(&t0, R_EDX); rtl_push(&t0);
    rtl_lr_l(&t0, R_EBX); rtl_push(&t0);
    rtl_lr_l(&t0, R_ESP); rtl_push(&t0);
    rtl_lr_l(&t0, R_EBP); rtl_push(&t0);
    rtl_lr_l(&t0, R_ESI); rtl_push(&t0);
    rtl_lr_l(&t0, R_EDI); rtl_push(&t0);
  }

  print_asm("pusha");
}

make_EHelper(popa) {
  if (decoding.is_operand_size_16) {
    rtl_pop(&t0); rtl_sr_w(R_DI, &t0);
    rtl_pop(&t0); rtl_sr_w(R_SI, &t0);
    rtl_pop(&t0); rtl_sr_w(R_BP, &t0);
    rtl_pop(&t0); rtl_addi(&cpu.esp, &cpu.esp, 2);
    rtl_pop(&t0); rtl_sr_w(R_BX, &t0);
    rtl_pop(&t0); rtl_sr_w(R_DX, &t0);
    rtl_pop(&t0); rtl_sr_w(R_CX, &t0);
    rtl_pop(&t0); rtl_sr_w(R_AX, &t0);
  }
  else {
    rtl_pop(&t0); rtl_sr_l(R_EDI, &t0);
    rtl_pop(&t0); rtl_sr_l(R_ESI, &t0);
    rtl_pop(&t0); rtl_sr_l(R_EBP, &t0);
    rtl_pop(&t0); rtl_addi(&cpu.esp, &cpu.esp, 4);
    rtl_pop(&t0); rtl_sr_l(R_EBX, &t0);
    rtl_pop(&t0); rtl_sr_l(R_EDX, &t0);
    rtl_pop(&t0); rtl_sr_l(R_ECX, &t0);
    rtl_pop(&t0); rtl_sr_l(R_EAX, &t0);
  }

  print_asm("popa");
}

make_EHelper(leave) {
  rtl_mv(&cpu.esp, &cpu.ebp);
  rtl_pop(&cpu.ebp);

  print_asm("leave");
}

make_EHelper(cltd) {
  if (decoding.is_operand_size_16) {
    rtl_lr_w(&t0, R_AX);
    rtl_msb(&t1, &t0, 2);
    rtl_sr_w(R_DX, &t1);
  }
  else {
    rtl_lr_l(&t0, R_EAX);
    rtl_msb(&t1, &t0, 4);
    rtl_sub(&t1, &tzero, &t1);
    rtl_sr_l(R_EDX, &t1);
  }

  print_asm(decoding.is_operand_size_16 ? "cwtl" : "cltd");
}

make_EHelper(cwtl) {
  if (decoding.is_operand_size_16) {
    rtl_lr_b(&t0, R_AL);
    rtl_sext(&t1, &t0, 1);
    rtl_sr_w(R_AX, &t1);
  }
  else {
    rtl_lr_w(&t0, R_AX);
    rtl_sext(&t1, &t0, 2);
    rtl_sr_l(R_EAX, &t1);
  }

  print_asm(decoding.is_operand_size_16 ? "cbtw" : "cwtl");
}

make_EHelper(movsx) {
  id_dest->width = decoding.is_operand_size_16 ? 2 : 4;
  rtl_sext(&t2, &id_src->val, id_src->width);
  operand_write(id_dest, &t2);
  print_asm_template2(movsx);
}

make_EHelper(movzx) {
  id_dest->width = decoding.is_operand_size_16 ? 2 : 4;
  operand_write(id_dest, &id_src->val);
  print_asm_template2(movzx);
}

make_EHelper(lea) {
  rtl_li(&t2, id_src->addr);
  operand_write(id_dest, &t2);
  print_asm_template2(lea);
}
