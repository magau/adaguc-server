/* 
 * Copyright (C) 2012, Royal Netherlands Meteorological Institute (KNMI)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or any 
 * later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Project    : ADAGUC
 *
 * initial programmer :  M.Plieger
 * initial date       :  20120610
 */
#ifndef CDEBUGGER_H_H2
#define CDEBUGGER_H_H2

#include <stdio.h>
#include <iostream>
#include <vector>

void _printDebugStream(const char* message);
void _printWarningStream(const char* message);
void _printErrorStream(const char* message);


void printDebugStream(const char* message);
void printWarningStream(const char* message);
void printErrorStream(const char* message);

void setDebugFunction(void (*function)(const char*) );
void setWarningFunction(void (*function)(const char*) );
void setErrorFunction(void (*function)(const char*) );

void _printDebugLine(const char *pszMessage,...);
void _printWarningLine(const char *pszMessage,...);
void _printErrorLine(const char *pszMessage,...);

void _printDebug(const char *pszMessage,...);
void _printWarning(const char *pszMessage,...);
void _printError(const char *pszMessage,...);

#define CDBWarning             _printWarning("[W: %s, %d in %s] ",__FILE__,__LINE__,className);_printWarningLine
#define CDBError               _printError("[E: %s, %d in %s] ",__FILE__,__LINE__,className);_printErrorLine
#define CDBErrormessage        _printErrorLine
#define CDBDebug               _printDebug("[D: %s, %d in %s] ",__FILE__,__LINE__,className);_printDebugLine
#define CDBEnterFunction(name) const char *functionName=name;_printDebugLine("D %s, %d class %s: Entering function '%s'",__FILE__,__LINE__,className,functionName);
#define CDBReturn(id)          {_printDebug("D %s, %d class %s::%s: returns %d\n",__FILE__,__LINE__,className,functionName,id);return id;}
#define CDBDebugFunction       _printDebug("D %s, %d class %s::%s: ",__FILE__,__LINE__,className,functionName);_printDebugLine
#define DEF_ERRORFUNCTION()    static const char *className;
#define DEF_ERRORMAIN()        static const char *className="main";



template <class myFreeType>
    void _allocateArray(myFreeType *object[], size_t elements,const char*file,const int line){
  if(*object!=NULL){
    delete[] *object;
    *object=NULL;
  }
#ifdef MEMLEAKCHECK
  *object = new(file,line)myFreeType[elements];
#else
  *object = new myFreeType[elements];
#endif
    }
      
#define allocateArray(object,size) _allocateArray(object,size,__FILE__,__LINE__)
//allocateArray(&nc.varids,nc.numvars);
template <class myFreeType>
    void deleteArray(myFreeType **object){
  if(*object!=NULL){
    delete[] *object;
    *object=NULL;
  }
}
template <class myFreeType>
    void deleteObject(myFreeType *object){
  if(*object!=NULL){
    delete *object;
    *object=NULL;
  }
}
    
#endif
