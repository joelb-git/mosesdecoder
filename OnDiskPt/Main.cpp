// $Id$
/***********************************************************************
 Moses - factored phrase-based, hierarchical and syntactic language decoder
 Copyright (C) 2009 Hieu Hoang

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 ***********************************************************************/

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>
#include <iterator>
#include <cassert>
#include "../moses/src/InputFileStream.h"
#include "../moses/src/Util.h"
#include "../moses/src/UserMessage.h"
#include "../OnDiskPt/OnDiskWrapper.h"
#include "../OnDiskPt/SourcePhrase.h"
#include "../OnDiskPt/TargetPhrase.h"
#include "../OnDiskPt/TargetPhraseCollection.h"
#include "../OnDiskPt/Word.h"
#include "../OnDiskPt/Vocab.h"
#include "Main.h"

using namespace std;
using namespace OnDiskPt;

int main (int argc, char * const argv[])
{
  // insert code here...
  Moses::ResetUserTime();
  Moses::PrintUserTime("Starting");

  assert(argc == 8);

  int numSourceFactors	= Moses::Scan<int>(argv[1])
     , numTargetFactors	= Moses::Scan<int>(argv[2])
     , numScores				= Moses::Scan<int>(argv[3])
     , tableLimit				= Moses::Scan<int>(argv[4]);
  TargetPhraseCollection::s_sortScoreInd			= Moses::Scan<int>(argv[5]);
  assert(TargetPhraseCollection::s_sortScoreInd < numScores);
  
  const string filePath 	= argv[6]
               ,destPath	= argv[7];


  Moses::InputFileStream inStream(filePath);

  OnDiskWrapper onDiskWrapper;
  bool retDb = onDiskWrapper.BeginSave(destPath, numSourceFactors, numTargetFactors, numScores);
  assert(retDb);

  PhraseNode &rootNode = onDiskWrapper.GetRootSourceNode();
  size_t lineNum = 0;
  char line[100000];

  //while(getline(inStream, line))
  while(inStream.getline(line, 100000)) {
    lineNum++;
    if (lineNum%1000 == 0) cerr << "." << flush;
    if (lineNum%10000 == 0) cerr << ":" << flush;
    if (lineNum%100000 == 0) cerr << lineNum << flush;
    //cerr << lineNum << " " << line << endl;

    std::vector<float> counts(1);
    string misc;
    SourcePhrase sourcePhrase;
    TargetPhrase *targetPhrase = new TargetPhrase(numScores);
    Tokenize(sourcePhrase, *targetPhrase, line, onDiskWrapper, numScores, counts, misc);
    assert(counts.size() == onDiskWrapper.GetNumCounts());

    rootNode.AddTargetPhrase(sourcePhrase, targetPhrase, onDiskWrapper, tableLimit, counts, misc);
  }

  rootNode.Save(onDiskWrapper, 0, tableLimit);
  onDiskWrapper.EndSave();

  Moses::PrintUserTime("Finished");

  //pause();
  return 0;

} // main()

bool Flush(const OnDiskPt::SourcePhrase *prevSourcePhrase, const OnDiskPt::SourcePhrase *currSourcePhrase)
{
  if (prevSourcePhrase == NULL)
    return false;

  assert(currSourcePhrase);
  bool ret = (*currSourcePhrase > *prevSourcePhrase);
  //cerr << *prevSourcePhrase << endl << *currSourcePhrase << " " << ret << endl << endl;

  return ret;
}

void Tokenize(SourcePhrase &sourcePhrase, TargetPhrase &targetPhrase, char *line, OnDiskWrapper &onDiskWrapper, int numScores, vector<float> &count, std::string &misc)
{
  size_t scoreInd = 0;
  stringstream miscBuf;

  // MAIN LOOP
  size_t stage = 0;
  /*	0 = source phrase
   1 = target phrase
   2 = scores
   3 = align
   4 = count
   */
  char *tok = strtok (line," ");
  while (tok != NULL) {
    if (0 == strcmp(tok, "|||")) {
      if (stage >= 5) {
        miscBuf << "|||" << " ";
      }
      ++stage;
    } else {
      switch (stage) {
      case 0: {
        Tokenize(sourcePhrase, tok, true, true, onDiskWrapper);
        break;
      }
      case 1: {
        Tokenize(targetPhrase, tok, false, true, onDiskWrapper);
        break;
      }
      case 2: {
        float score = Moses::Scan<float>(tok);
        targetPhrase.SetScore(score, scoreInd);
        ++scoreInd;
        break;
      }
      case 3: {
        targetPhrase.Create1AlignFromString(tok);
        break;
      }
      case 4: {
        // count info. Only store the 2nd one
        float val = Moses::Scan<float>(tok);
        count[0] = val;
        break;
      }
      default:
        miscBuf << tok << " ";
        break;
      }
    }

    tok = strtok (NULL, " ");
  } // while (tok != NULL)

  assert(scoreInd == numScores);
  
  misc = miscBuf.str();
  misc = Moses::Trim(misc);
  targetPhrase.SetMisc(misc);
  
  targetPhrase.SortAlign();

} // Tokenize()

void Tokenize(OnDiskPt::Phrase &phrase
              , const std::string &token, bool addSourceNonTerm, bool addTargetNonTerm
              , OnDiskPt::OnDiskWrapper &onDiskWrapper)
{

  bool nonTerm = false;
  size_t tokSize = token.size();
  int comStr =token.compare(0, 1, "[");

  if (comStr == 0) {
    comStr = token.compare(tokSize - 1, 1, "]");
    nonTerm = comStr == 0;
  }

  if (nonTerm) {
    // non-term
    size_t splitPos		= token.find_first_of("[", 2);
    string wordStr	= token.substr(0, splitPos);

    if (splitPos == string::npos) {
      // lhs - only 1 word
      Word *word = new Word();
      word->CreateFromString(wordStr, onDiskWrapper.GetVocab());
      phrase.AddWord(word);
    } else {
      // source & target non-terms
      if (addSourceNonTerm) {
        Word *word = new Word();
        word->CreateFromString(wordStr, onDiskWrapper.GetVocab());
        phrase.AddWord(word);
      }

      wordStr = token.substr(splitPos, tokSize - splitPos);
      if (addTargetNonTerm) {
        Word *word = new Word();
        word->CreateFromString(wordStr, onDiskWrapper.GetVocab());
        phrase.AddWord(word);
      }

    }
  } else {
    // term
    Word *word = new Word();
    word->CreateFromString(token, onDiskWrapper.GetVocab());
    phrase.AddWord(word);
  }
}

void InsertTargetNonTerminals(std::vector<std::string> &sourceToks, const std::vector<std::string> &targetToks, const ::AlignType &alignments)
{
  for (int ind = alignments.size() - 1; ind >= 0; --ind) {
    const ::AlignPair &alignPair = alignments[ind];
    size_t sourcePos = alignPair.first
                       ,targetPos = alignPair.second;

    const string &target = targetToks[targetPos];
    sourceToks.insert(sourceToks.begin() + sourcePos + 1, target);

  }
}

class AlignOrderer
{
public:
  bool operator()(const ::AlignPair &a, const ::AlignPair &b) const {
    return a.first < b.first;
  }
};

void SortAlign(::AlignType &alignments)
{
  std::sort(alignments.begin(), alignments.end(), AlignOrderer());
}
