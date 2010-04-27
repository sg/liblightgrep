#pragma once

#include "basic.h"

struct Instruction;

struct Thread {
  Thread(): PC(0), Label(0), Start(0), End(0) {}
  Thread(const Instruction* pc, uint32 label, uint64 start, uint64 end): PC(pc), Label(label), Start(start), End(end) {}

  const Instruction* PC;
  uint32             Label;
  uint64             Start,
                     End;

  bool operator==(const Thread& x) const {
    return PC == x.PC && Label == x.Label && Start == x.Start && End == x.End;
  }

};

std::ostream& operator<<(std::ostream& out, const Thread& t);

typedef std::vector< Thread > ThreadList;