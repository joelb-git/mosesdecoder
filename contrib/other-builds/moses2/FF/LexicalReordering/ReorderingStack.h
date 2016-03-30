/*
 * ReorderingStack.h
 ** Author: Ankit K. Srivastava
 ** Date: Jan 26, 2010
 */

#pragma once

//#include <string>
#include <vector>
//#include "Factor.h"
//#include "Phrase.h"
//#include "TypeDef.h"
//#include "Util.h"
#include "../../legacy/Range.h"

namespace Moses2
{

/** @todo what is this?
 */
class ReorderingStack
{
private:

  std::vector<Range> m_stack;

public:

  size_t hash() const;
  bool operator==(const ReorderingStack& other) const;

  void Init();
  int ShiftReduce(Range input_span);

private:
  void Reduce(Range input_span);
};


}
