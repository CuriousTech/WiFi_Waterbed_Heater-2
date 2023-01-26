// Helper class for json strings

class jsonString
{
public:
  jsonString(const char *pLabel = NULL)
  {
    m_cnt = 0;
    if(pLabel)
      s = pLabel, s += ";";
    s += "{";
  }
        
  String Close(void)
  {
    s += "}";
    return s;
  }

  void Var(const char *key, int iVal)
  {
    if(m_cnt) s += ",";
    s += "\"";
    s += key;
    s += "\":";
    s += iVal;
    m_cnt++;
  }

  void Var(const char *key, uint32_t iVal)
  {
    if(m_cnt) s += ",";
    s += "\"";
    s += key;
    s += "\":";
    s += iVal;
    m_cnt++;
  }

  void Var(const char *key, long int iVal)
  {
    if(m_cnt) s += ",";
    s += "\"";
    s += key;
    s += "\":";
    s += iVal;
    m_cnt++;
  }

  void Var(const char *key, float fVal)
  {
    if(m_cnt) s += ",";
    s += "\"";
    s += key;
    s += "\":";
    s += fVal;
    m_cnt++;
  }
  
  void Var(const char *key, bool bVal)
  {
    if(m_cnt) s += ",";
    s += "\"";
    s += key;
    s += "\":";
    s += bVal ? 1:0;
    m_cnt++;
  }
  
  void Var(const char *key, char *sVal)
  {
    if(m_cnt) s += ",";
    s += "\"";
    s += key;
    s += "\":\"";
    s += sVal;
    s += "\"";
    m_cnt++;
  }
  
  void Var(const char *key, String sVal)
  {
    if(m_cnt) s += ",";
    s += "\"";
    s += key;
    s += "\":\"";
    s += sVal;
    s += "\"";
    m_cnt++;
  }

  void Array(const char *key, uint8_t iVal[], int n)
  {
    if(m_cnt) s += ",";
    s += "\"";
    s += key;
    s += "\":[";
    for(int i = 0; i < n; i++)
    {
      if(i) s += ",";
      s += iVal[i];
    }
    s += "]";
    m_cnt++;
  }

  void Array(const char *key, uint16_t iVal[], int n)
  {
    if(m_cnt) s += ",";
    s += "\"";
    s += key;
    s += "\":[";
    for(int i = 0; i < n; i++)
    {
      if(i) s += ",";
      s += iVal[i];
    }
    s += "]";
    m_cnt++;
  }

  void Array(const char *key, uint32_t iVal[], int n)
  {
    if(m_cnt) s += ",";
    s += "\"";
    s += key;
    s += "\":[";
    for(int i = 0; i < n; i++)
    {
      if(i) s += ",";
      s += iVal[i];
    }
    s += "]";
    m_cnt++;
  }

 // custom arrays for waterbed
  void Array(const char *key, Sched sVal[][MAX_SCHED], int n)
  {
    if(m_cnt) s += ",";
    s += "\"";
    s += key;
    s += "\":[";

    for(int i2 = 0; i2 < n; i2++)
    {
      if(i2) s += ",";
      s += "[";
      for(int i = 0; i < MAX_SCHED; i++)
      {
        if(i) s += ",";
        s += "[";
        s += sVal[i2][i].timeSch;
        s += ","; s += String( (float)sVal[i2][i].setTemp/10, 1 );
        s += ","; s += String( (float)sVal[i2][i].thresh/10,1 );
        s += "]";
      }
      s += "]";
    }
    s += "]";
    m_cnt++;
  }

  void ArrayCost(const char *key, uint16_t iVal[], int n)
  {
    if(m_cnt) s += ",";
    s += "\"";
    s += key;
    s += "\":[";
    for(int i = 0; i < n; i++)
    {
      if(i) s += ",";
      s += (float)iVal[i]/100;
    }
    s += "]";
    m_cnt++;
  }

protected:
  String s;
  int m_cnt;
};
