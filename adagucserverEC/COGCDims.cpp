/******************************************************************************
 * 
 * Project:  ADAGUC Server
 * Purpose:  ADAGUC OGC Server
 * Author:   Maarten Plieger, plieger "at" knmi.nl
 * Date:     2013-06-01
 *
 ******************************************************************************
 *
 * Copyright 2013, Royal Netherlands Meteorological Institute (KNMI)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *      http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * 
 ******************************************************************************/

#include "COGCDims.h"
const char * CCDFDims::className = "CCDFDims";

void COGCDims::addValue(const char *value){
  for(size_t j=0;j<uniqueValues.size();j++){
    if(uniqueValues[j].equals(value))return;
  }
  uniqueValues.push_back(value);
  
}

CCDFDims::~CCDFDims (){
  for(size_t j=0;j<dimensions.size();j++){
    delete dimensions[j];
    dimensions[j]=NULL;
  }
}

void CCDFDims::addDimension(const char *name,const char *value,size_t index){
  //CDBDebug("Adddimension %s %s %d",name,value,index);
  for(size_t j=0;j<dimensions.size();j++){
    if(dimensions[j]->name.equals(name)){
      dimensions[j]->index=index;
      dimensions[j]->value=value;
      return;
    }
  }
  NetCDFDim *dim = new NetCDFDim;
  dim->name.copy(name);
  dim->value.copy(value);
  dim->index=index;
  dimensions.push_back(dim);
}
size_t CCDFDims::getDimensionIndex(const char *name){
  for(size_t j=0;j<dimensions.size();j++){
    if(dimensions[j]->name.equals(name)){
      return dimensions[j]->index;
    }
  }
  return 0;
}
size_t CCDFDims::getDimensionIndex(int j){
  if(j<0)return 0;
  if(size_t(j)>dimensions.size())return 0;
  return dimensions[j]->index;
}
const char *CCDFDims::getDimensionValue(int j){
  if(j<0)return 0;
  if(size_t(j)>dimensions.size())return 0;
  return dimensions[j]->value.c_str();
}
const char *CCDFDims::getDimensionName(int j){
  if(j<0)return 0;
  if(size_t(j)>dimensions.size())return 0;
  return dimensions[j]->name.c_str();
}