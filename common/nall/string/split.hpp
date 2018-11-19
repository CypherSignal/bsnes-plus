#ifndef NALL_STRING_SPLIT_HPP
#define NALL_STRING_SPLIT_HPP

namespace nall {

template<unsigned Limit> void lstring::split(const char *key, const char *src) {
  unsigned limit = Limit;
  reset();

  int ssl = strlen(src), ksl = strlen(key);
  int lp = 0;

  // scan through the src string and find how many and where all of the splits should occur
  linear_vector<const char*> splitLocations;
  linear_vector<unsigned int> splitLengths;
  for(int i = 0; i <= ssl - ksl;)
  {
    if(!memcmp(src + i, key, ksl))
    {
      splitLocations.append(src + lp);
      splitLengths.append(i - lp);
      i += ksl;
      lp = i;
      if (!--limit)
      {
        break;
      }
    } 
    else
    {
      i++;
    }
  }

  // do a single reserve of the entire lstring, and copy the contents of each string one-at-a-time
  resize(splitLocations.size()+1);
  for (int i = 0; i < splitLocations.size(); ++i)
  {
    string& str = operator[](i);
    unsigned int splitLength = splitLengths[i];
    
    str.reserve(splitLength);
    memcpy((void*)str(), (void*)splitLocations[i], splitLength);
    str[splitLength] = 0;
  }

  operator[](splitLocations.size()) = src + lp;
}

template<unsigned Limit> void lstring::qsplit(const char *key, const char *src) {
  unsigned limit = Limit;
  reset();

  int ssl = strlen(src), ksl = strlen(key);
  int lp = 0, split_count = 0;

  for(int i = 0; i <= ssl - ksl;) {
    uint8_t x = src[i];

    if(x == '\"' || x == '\'') {
      int z = i++;                        //skip opening quote
      while(i < ssl && src[i] != x) i++;
      if(i >= ssl) i = z;                 //failed match, rewind i
      else {
        i++;                              //skip closing quote
        continue;                         //restart in case next char is also a quote
      }
    }

    if(!memcmp(src + i, key, ksl)) {
      strlcpy(operator[](split_count++), src + lp, i - lp + 1);
      i += ksl;
      lp = i;
      if(!--limit) break;
    } else i++;
  }

  operator[](split_count++) = src + lp;
}

};

#endif
