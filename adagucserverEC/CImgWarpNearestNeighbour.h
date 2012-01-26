#ifndef CImgWarpNearestNeighbour_H
#define CImgWarpNearestNeighbour_H
#include <float.h>
#include <pthread.h>
#include "CImageWarperRenderInterface.h"

/**
  This interface represents the tile rendering classes.
*/
class CDrawTileObjInterface{
  public:
    virtual void init(CDataSource *dataSource,CDrawImage *drawImage,int tileWidth,int tileHeight) = 0;
    virtual int drawTile(double *x_corners,double *y_corners,int &dDestX,int &dDestY) = 0;
    virtual ~CDrawTileObjInterface(){
    }
};

/**
  This tile rendering class stores calculated results in a byte field. 
  When the same source pixel of the source dataset is requested again, its values can be retrieved from the bytecache.
  This is slow for very large datasets, because the bytecachefield needs to have the same dimensions as the source dataset.
  This one is especially usefull on int, float and double types of datasources.
*/
class CDrawTileObjByteCache:public CDrawTileObjInterface{
  private:
        DEF_ERRORFUNCTION();
  public:
    float x_div,y_div;
    float dfSourceBBOX[4],dfImageBBOX[4];
    float dfNodataValue;
    float legendLowerRange;
    float legendUpperRange;
    bool legendValueRange;
    bool hasNodataValue;
    int dWidth,dHeight;
    float legendLog,legendScale,legendOffset;
    unsigned char *buf;
    CDataSource * dataSource;
    CDrawImage *drawImage;
    void init(CDataSource *dataSource,CDrawImage *drawImage,int tileWidth,int tileHeight){
      this->dataSource = dataSource;
      this->drawImage = drawImage;
      x_div=tileWidth;y_div=tileHeight;
      for(int k=0;k<4;k++){
        dfSourceBBOX[k]=dataSource->dfBBOX[k];
        dfImageBBOX[k]=dataSource->dfBBOX[k];
      }
      
      //Look whether BBOX was swapped in y dir
      if(dataSource->dfBBOX[3]<dataSource->dfBBOX[1]){
        dfSourceBBOX[1]=dataSource->dfBBOX[3];
        dfSourceBBOX[3]=dataSource->dfBBOX[1];
      }
      //Look whether BBOX was swapped in x dir
      if(dataSource->dfBBOX[2]<dataSource->dfBBOX[0]){
        dfSourceBBOX[0]=dataSource->dfBBOX[2];
        dfSourceBBOX[2]=dataSource->dfBBOX[0];
      }
      
      dfNodataValue    = dataSource->dataObject[0]->dfNodataValue ;
      legendValueRange = dataSource->legendValueRange;
      legendLowerRange = dataSource->legendLowerRange;
      legendUpperRange = dataSource->legendUpperRange;
      hasNodataValue   = dataSource->dataObject[0]->hasNodataValue;
      dWidth = dataSource->dWidth;
      dHeight = dataSource->dHeight;
      legendLog = dataSource->legendLog;
      legendScale = dataSource->legendScale;
      legendOffset = dataSource->legendOffset;
      // Allocate the Byte Buffer
      size_t imageSize=dataSource->dWidth*dataSource->dHeight;
      allocateArray(&buf,imageSize);
      memset ( buf, 255, imageSize);
    }
    CDrawTileObjByteCache(){
      buf=NULL;
    }
    ~CDrawTileObjByteCache(){
      deleteArray(&buf);
    }
    int drawTile(double *x_corners,double *y_corners,int &dDestX,int &dDestY){
      CDFType dataType=dataSource->dataObject[0]->dataType;
      void *data=dataSource->dataObject[0]->data;
      switch(dataType){
        case CDF_CHAR  : return myDrawRawTile((char*)data,x_corners,y_corners,dDestX,dDestY);break;
        case CDF_BYTE  : return myDrawRawTile((char*)data,x_corners,y_corners,dDestX,dDestY);break;
        case CDF_UBYTE : return myDrawRawTile((unsigned char*)data,x_corners,y_corners,dDestX,dDestY);break;
        case CDF_SHORT : return myDrawRawTile((short*)data,x_corners,y_corners,dDestX,dDestY);break;
        case CDF_USHORT: return myDrawRawTile((ushort*)data,x_corners,y_corners,dDestX,dDestY);break;
        case CDF_INT   : return myDrawRawTile((int*)data,x_corners,y_corners,dDestX,dDestY);break;
        case CDF_UINT  : return myDrawRawTile((uint*)data,x_corners,y_corners,dDestX,dDestY);break;
        case CDF_FLOAT : return myDrawRawTile((float*)data,x_corners,y_corners,dDestX,dDestY);break;
        case CDF_DOUBLE: return myDrawRawTile((double*)data,x_corners,y_corners,dDestX,dDestY);break;
      }
      return 1;
    }
    template <class T>
    int myDrawRawTile(T*data,double *x_corners,double *y_corners,int &dDestX,int &dDestY){
      float sample_sy,sample_sx;
      float line_dx1,line_dy1,line_dx2,line_dy2;
      float rcx_1,rcy_1,rcx_2,rcy_2,rcx_3,rcy_3;
      int x,y;
      int srcpixel_x,srcpixel_y;
      int dstpixel_x,dstpixel_y;
      int k;
      rcx_1= (x_corners[0] - x_corners[3])/x_div;
      rcy_1= (y_corners[0] - y_corners[3])/x_div;
      rcx_2= (x_corners[1] - x_corners[2])/x_div;
      rcy_2= (y_corners[1] - y_corners[2])/x_div;
      
      for(k=0;k<4;k++)
        if(fabs(x_corners[k]-x_corners[0])>=fabs(dfSourceBBOX[2]-dfSourceBBOX[0]))break;
      if(k==4){
        for(k=0;k<4;k++)
          if(x_corners[k]>dfSourceBBOX[0]&&x_corners[k]<dfSourceBBOX[2])break;
        if(k==4){
          return __LINE__;
        }
      }
      for(k=0;k<4;k++)
        if(fabs(y_corners[k]-y_corners[0])>=fabs(dfSourceBBOX[3]-dfSourceBBOX[1]))break;
      if(k==4){
        for(k=0;k<4;k++)
          if(y_corners[k]>dfSourceBBOX[1]&&y_corners[k]<dfSourceBBOX[3])break;
        if(k==4)return __LINE__;
      }
      line_dx1= x_corners[3];
      line_dx2= x_corners[2];
      line_dy1= y_corners[3];
      line_dy2= y_corners[2];
      bool isNodata=false;
      float val;
      size_t imgpointer;
      for(x=0;x<=x_div;x++){
        line_dx1+=rcx_1;line_dx2+=rcx_2;line_dy1+=rcy_1;line_dy2+=rcy_2;
        rcx_3= (line_dx2 -line_dx1)/y_div;
        rcy_3= (line_dy2 -line_dy1)/y_div;
        dstpixel_x=int(x)+dDestX;
        for(y=0;y<=y_div;y=y+1){
          dstpixel_y=int(y)+dDestY;
          sample_sx=line_dx1+rcx_3*y;
          if(sample_sx>=dfSourceBBOX[0]&&sample_sx<dfSourceBBOX[2]){
            sample_sy=line_dy1+rcy_3*y;
            if(sample_sy>=dfSourceBBOX[1]&&sample_sy<dfSourceBBOX[3]){
              srcpixel_x=int(((sample_sx-dfImageBBOX[0])/(dfImageBBOX[2]-dfImageBBOX[0]))*dWidth);
              if(srcpixel_x>=0&&srcpixel_x<dWidth){
                srcpixel_y=int(((sample_sy-dfImageBBOX[1])/(dfImageBBOX[3]-dfImageBBOX[1]))*dHeight);
                if(srcpixel_y>=0&&srcpixel_y<dHeight){
                    imgpointer=srcpixel_x+(dHeight-1-srcpixel_y)*dWidth;
                  //254 Means nodata
                  //255 Means not been there yet.
                  if(buf[imgpointer]!=254){
                    if(buf[imgpointer]==255){
                      val=data[imgpointer];
                      isNodata=false;
                      //if(x==10&&y==10)CDBDebug("val ==(T)dfNodataValue %f==%f  (%d) %d",val,dfNodataValue,(val==dfNodataValue),hasNodataValue);
                      if(hasNodataValue){if(val==dfNodataValue)isNodata=true;else if(!(val==val))isNodata=true;}
                      if(!isNodata)if(legendValueRange)if(val<legendLowerRange||val>legendUpperRange)isNodata=true;
                      if(!isNodata){
                        if(legendLog!=0){
                          if(val==0)val=1e-10;
                          val=log10(val)/log10(legendLog);
                        }
                        val*=legendScale;
                        val+=legendOffset;
                        if(val>=239)val=239;else if(val<0)val=0;
                        buf[imgpointer]=(unsigned char)val;
                        //drawImage->setPixelIndexed(dstpixel_x,dstpixel_y,drawImage->colors[buf[imgpointer]]);
                        drawImage->setPixelIndexed(dstpixel_x,dstpixel_y,buf[imgpointer]);
                      }else buf[imgpointer]=254;
                    }else{
                      //drawImage->setPixelIndexed(dstpixel_x,dstpixel_y,drawImage->colors[buf[imgpointer]]);
                      drawImage->setPixelIndexed(dstpixel_x,dstpixel_y,buf[imgpointer]);
                    }
                  }
                }
              }
            }
          }
        }
      }
      return 0;
    }
};

/**
  This tile just runs over the datasource field, and calculates the destination pixel color over and over again when it is requested twice.
  This class is very fast for large datasets, with low zoom levels (zoomed out completely)
*/
class CDrawTileObj:public CDrawTileObjInterface{
  public:
    float x_div,y_div;
    float dfSourceBBOX[4],dfImageBBOX[4];
    float dfNodataValue;
    float legendLowerRange;
    float legendUpperRange;
    bool legendValueRange;
    bool hasNodataValue;
    int dWidth,dHeight;
    float legendLog,legendScale,legendOffset;
    CDataSource * dataSource;
    CDrawImage *drawImage;
//size_t prev_imgpointer;
    void init(CDataSource *dataSource,CDrawImage *drawImage,int tileWidth,int tileHeight){
      this->dataSource = dataSource;
      this->drawImage = drawImage;
      x_div=tileWidth;y_div=tileHeight;
      for(int k=0;k<4;k++){
        dfSourceBBOX[k]=dataSource->dfBBOX[k];
        dfImageBBOX[k]=dataSource->dfBBOX[k];
      }
      
      //Look whether BBOX was swapped in y dir
      if(dataSource->dfBBOX[3]<dataSource->dfBBOX[1]){
        dfSourceBBOX[1]=dataSource->dfBBOX[3];
        dfSourceBBOX[3]=dataSource->dfBBOX[1];
      }
      //Look whether BBOX was swapped in x dir
      if(dataSource->dfBBOX[2]<dataSource->dfBBOX[0]){
        dfSourceBBOX[0]=dataSource->dfBBOX[2];
        dfSourceBBOX[2]=dataSource->dfBBOX[0];
      }
      
      dfNodataValue    = dataSource->dataObject[0]->dfNodataValue ;
      legendValueRange = dataSource->legendValueRange;
      legendLowerRange = dataSource->legendLowerRange;
      legendUpperRange = dataSource->legendUpperRange;
      hasNodataValue   = dataSource->dataObject[0]->hasNodataValue;
      dWidth = dataSource->dWidth;
      dHeight = dataSource->dHeight;
      legendLog = dataSource->legendLog;
      legendScale = dataSource->legendScale;
      legendOffset = dataSource->legendOffset;
    }
      int drawTile(double *x_corners,double *y_corners,int &dDestX,int &dDestY){
      CDFType dataType=dataSource->dataObject[0]->dataType;
      void *data=dataSource->dataObject[0]->data;
      switch(dataType){
        case CDF_CHAR  : return myDrawRawTile((char*)data,x_corners,y_corners,dDestX,dDestY);break;
        case CDF_BYTE  : return myDrawRawTile((char*)data,x_corners,y_corners,dDestX,dDestY);break;
        case CDF_UBYTE : return myDrawRawTile((unsigned char*)data,x_corners,y_corners,dDestX,dDestY);break;
        case CDF_SHORT : return myDrawRawTile((short*)data,x_corners,y_corners,dDestX,dDestY);break;
        case CDF_USHORT: return myDrawRawTile((ushort*)data,x_corners,y_corners,dDestX,dDestY);break;
        case CDF_INT   : return myDrawRawTile((int*)data,x_corners,y_corners,dDestX,dDestY);break;
        case CDF_UINT  : return myDrawRawTile((uint*)data,x_corners,y_corners,dDestX,dDestY);break;
        case CDF_FLOAT : return myDrawRawTile((float*)data,x_corners,y_corners,dDestX,dDestY);break;
        case CDF_DOUBLE: return myDrawRawTile((double*)data,x_corners,y_corners,dDestX,dDestY);break;
      }
      return 1;
    }
    template <class T>
    int myDrawRawTile(T*data,double *x_corners,double *y_corners,int &dDestX,int &dDestY){
      float sample_sy,sample_sx;
      float line_dx1,line_dy1,line_dx2,line_dy2;
      float rcx_1,rcy_1,rcx_2,rcy_2,rcx_3,rcy_3;
      int x,y;
      int srcpixel_x,srcpixel_y;
      int dstpixel_x,dstpixel_y;
      int k;
      rcx_1= (x_corners[0] - x_corners[3])/x_div;
      rcy_1= (y_corners[0] - y_corners[3])/x_div;
      rcx_2= (x_corners[1] - x_corners[2])/x_div;
      rcy_2= (y_corners[1] - y_corners[2])/x_div;
      for(k=0;k<4;k++)
        if(fabs(x_corners[k]-x_corners[0])>=fabs(dfSourceBBOX[2]-dfSourceBBOX[0]))break;
      if(k==4){
        for(k=0;k<4;k++)
          if(x_corners[k]>dfSourceBBOX[0]&&x_corners[k]<dfSourceBBOX[2])break;
        if(k==4)return __LINE__;
      }
      for(k=0;k<4;k++)
        if(fabs(y_corners[k]-y_corners[0])>=fabs(dfSourceBBOX[3]-dfSourceBBOX[1]))break;
      if(k==4){
        for(k=0;k<4;k++)
          if(y_corners[k]>dfSourceBBOX[1]&&y_corners[k]<dfSourceBBOX[3])break;
        if(k==4)return __LINE__;
      }

      line_dx1= x_corners[3];
      line_dx2= x_corners[2];
      line_dy1= y_corners[3];
      line_dy2= y_corners[2];
      bool isNodata=false;
      float val;
      size_t imgpointer;
      for(x=0;x<=x_div;x++){
        line_dx1+=rcx_1;line_dx2+=rcx_2;line_dy1+=rcy_1;line_dy2+=rcy_2;
        rcx_3= (line_dx2 -line_dx1)/y_div;
        rcy_3= (line_dy2 -line_dy1)/y_div;
        dstpixel_x=int(x)+dDestX;
        for(y=0;y<=y_div;y=y+1){
          dstpixel_y=int(y)+dDestY;
          sample_sx=line_dx1+rcx_3*y;
          if(sample_sx>=dfSourceBBOX[0]&&sample_sx<dfSourceBBOX[2])
          {
            sample_sy=line_dy1+rcy_3*y;
            if(sample_sy>=dfSourceBBOX[1]&&sample_sy<dfSourceBBOX[3])
            {
              srcpixel_x=int(((sample_sx-dfImageBBOX[0])/(dfImageBBOX[2]-dfImageBBOX[0]))*dWidth);
              if(srcpixel_x>=0&&srcpixel_x<dWidth){
                srcpixel_y=int(((sample_sy-dfImageBBOX[1])/(dfImageBBOX[3]-dfImageBBOX[1]))*dHeight);
                if(srcpixel_y>=0&&srcpixel_y<dHeight)
                {
                  imgpointer=srcpixel_x+(dHeight-1-srcpixel_y)*dWidth;
                    imgpointer=srcpixel_x+(dHeight-1-srcpixel_y)*dWidth;
                  val=data[imgpointer];
                  isNodata=false;
                  if(hasNodataValue){if(val==dfNodataValue)isNodata=true;else if(!(val==val))isNodata=true;}
                  if(!isNodata)if(legendValueRange)if(val<legendLowerRange||val>legendUpperRange)isNodata=true;
                  if(!isNodata){
                    if(legendLog!=0){
                      if(val==0)val=1e-10;
                      val=log10(val)/log10(legendLog);
                    }
                    val*=legendScale;
                    val+=legendOffset;
                    if(val>=239)val=239;else if(val<0)val=0;
                    //drawImage->setPixelIndexed(dstpixel_x,dstpixel_y,drawImage->colors[(unsigned char)val]);
                    drawImage->setPixelIndexed(dstpixel_x,dstpixel_y,(int)val);
                  }
                }
              }
            }
          }
        }
      }
      return 0;
    }
};

/**
  This is the main class of this file. It renders the sourcedata on the destination image using nearest neighbour interpolation.
  It uses tile blocks to render the data. Only the corners of these tiles are projected. Not each source pixel is projected,
  because this is uneccessary time consuming for a visualization.
*/
class CImgWarpNearestNeighbour:public CImageWarperRenderInterface{
  private:
    DEF_ERRORFUNCTION();
    int set(const char *settings){
      return 0;
    }
    int status;

    //This class represents settings/properties for each separate tile
    class DrawTileSettings{
      public:
        int id;
        double y_corners[4],x_corners[4];
        int tile_offset_x,tile_offset_y;
        CDrawTileObjInterface *drawTile;
    };
    //This class represents which bunch of tiles needs to be drawn by which thread.
    class DrawMultipleTileSettings{
      public:
        DrawTileSettings *ct;
        int numberOfTiles,startTile,endTile;
    };
    //This method draws th bunch of selected tiles with the proper renderer.
    //This method can be called by a thread.
    static void *drawTiles(void *arg){
      DrawMultipleTileSettings *dmf = (DrawMultipleTileSettings *)arg;
      for(int j=dmf->startTile;j<dmf->endTile&&j<dmf->numberOfTiles;j++){
        DrawTileSettings *ct = &dmf->ct[j];
        if(ct->id>=0){
          ct->drawTile->drawTile(ct->x_corners,ct->y_corners,ct->tile_offset_x,ct->tile_offset_y);
        }
      }
      return arg;
    }
    //Reproject the corners of the tiles
     double y_corners[4],x_corners[4];
    double dfMaskBBOX[4];
    int reproj(CImageWarper *warper,CDataSource *dataSource,CGeoParams *GeoDest,double dfx,double dfy,double x_div,double y_div){
      double psx[4];
      double psy[4];
      double dfTiledBBOX[4];
      double dfTileW=(GeoDest->dfBBOX[2]-GeoDest->dfBBOX[0])/double(x_div);
      double dfTileH=(GeoDest->dfBBOX[3]-GeoDest->dfBBOX[1])/double(y_div);

      dfTiledBBOX[0]=GeoDest->dfBBOX[0]+dfTileW*dfx;
      dfTiledBBOX[1]=GeoDest->dfBBOX[1]+dfTileH*dfy;
      dfTiledBBOX[2]=dfTiledBBOX[0]+(dfTileW);
      dfTiledBBOX[3]=dfTiledBBOX[1]+(dfTileH);
      double dfSourceBBOX[4];
      for(int k=0;k<4;k++)dfSourceBBOX[k]=dfMaskBBOX[k];
      if(dfMaskBBOX[3]<dfMaskBBOX[1]){
        dfSourceBBOX[1]=dfMaskBBOX[3];
        dfSourceBBOX[3]=dfMaskBBOX[1];
      }
      if(
         (dfTiledBBOX[0]>dfSourceBBOX[0]-dfTileW&&dfTiledBBOX[0]<dfSourceBBOX[2]+dfTileW)&&
         (dfTiledBBOX[2]>dfSourceBBOX[0]-dfTileW&&dfTiledBBOX[2]<dfSourceBBOX[2]+dfTileW)&&
         (dfTiledBBOX[1]>dfSourceBBOX[1]-dfTileH&&dfTiledBBOX[1]<dfSourceBBOX[3]+dfTileH)&&
         (dfTiledBBOX[3]>dfSourceBBOX[1]-dfTileH&&dfTiledBBOX[3]<dfSourceBBOX[3]+dfTileH)
        )
        ;else return 1;
      psx[0]=dfTiledBBOX[2];
      psx[1]=dfTiledBBOX[2];
      psx[2]=dfTiledBBOX[0];
      psx[3]=dfTiledBBOX[0];
      psy[0]=dfTiledBBOX[1];
      psy[1]=dfTiledBBOX[3];
      psy[2]=dfTiledBBOX[3];
      psy[3]=dfTiledBBOX[1];
      CT::string destinationCRS;
      warper->decodeCRS(&destinationCRS,&GeoDest->CRS);
      if(destinationCRS.indexOf("longlat")>=0){
        for(int j=0;j<4;j++){
         psx[j]*=DEG_TO_RAD;
          psy[j]*=DEG_TO_RAD;
        }
      }
        pj_transform(warper->destpj,warper->sourcepj, 4,0,psx,psy,NULL);
        if(dataSource->nativeProj4.indexOf("longlat")>=0)
          for(int j=0;j<4;j++){
          psx[j]/=DEG_TO_RAD;
          psy[j]/=DEG_TO_RAD;
          }
          x_corners[0]=psx[1];
          y_corners[0]=psy[1];

          x_corners[1]=psx[0];
          y_corners[1]=psy[0];

          x_corners[2]=psx[3];
          y_corners[2]=psy[3];

          x_corners[3]=psx[2];
          y_corners[3]=psy[2];
          return 0;
    }

    //Setup projection and all other settings for the tiles to draw
    void render(CImageWarper *warper,CDataSource *dataSource,CDrawImage *drawImage){
      //CDBDebug("Render");
      //This enables wether bunches of tiles are divided allong threads.
      int numThreads=4;
      //Threading is not needed when only one thread is specified.
      bool useThreading=true;
      if(numThreads==1)useThreading=false;
      
      warper->findExtent(dataSource,dfMaskBBOX);
      int x_div=drawImage->Geo->dWidth/16;
      int y_div=drawImage->Geo->dHeight/16;
      if(warper->isProjectionRequired()==false){
         //CDBDebug("No reprojection required");
         x_div=1;
         y_div=1;
         //When we are drawing just one tile, threading is not needed
         useThreading=false;
      }
      double tile_width=(double(drawImage->Geo->dWidth)/double(x_div));
      double tile_height=(double(drawImage->Geo->dHeight)/double(y_div));

      //Setup the renderer to draw the tiles with.We do not keep the calculated results for CDF_CHAR (faster)
      CDrawTileObjInterface *drawTileClass= NULL;
      if(dataSource->dataObject[0]->dataType==CDF_CHAR||
        dataSource->dataObject[0]->dataType==CDF_BYTE||
        dataSource->dataObject[0]->dataType==CDF_UBYTE
      ){
        drawTileClass = new CDrawTileObj();           //Do not keep the calculated results for CDF_CHAR
        
      }else{
        //drawTileClass = new CDrawTileObjByteCache();  //keep the calculated results
        drawTileClass = new CDrawTileObj();           //Do not keep the calculated results for CDF_CHAR
      }
      //drawTileClass = new CDrawTileObjByteCache();           //Do not keep the calculated results for CDF_CHAR
      //drawTileClass = new CDrawTileObj();  //keep the calculated results
      drawTileClass->init(dataSource,drawImage,(int)tile_width,(int)tile_height);
      //CDBDebug("b %f %f %f %f",dataSource->dfBBOX[0],dataSource->dfBBOX[1],dataSource->dfBBOX[2],dataSource->dfBBOX[3]);
      //CDBDebug("d %f %f %f %f",drawImage->Geo->dfBBOX[0],drawImage->Geo->dfBBOX[1],drawImage->Geo->dfBBOX[2],drawImage->Geo->dfBBOX[3]);
      
      int numberOfTiles=x_div*y_div;
      DrawTileSettings *drawTileSettings = new DrawTileSettings[numberOfTiles];
      DrawTileSettings *curTileSettings;
      for(int x=0;x<x_div;x++){
        for(int y=0;y<y_div;y++){
        status = reproj(warper,dataSource,drawImage->Geo,x,(y_div-1)-y,x_div,y_div);
        int tileId=x+y*x_div;
        curTileSettings=&drawTileSettings[tileId];
        curTileSettings->id=tileId;
        if(status != 0)curTileSettings->id=(-tileId)-1;//This one does not need to be done.
        for(int j=0;j<4;j++){
          curTileSettings->x_corners[j]=x_corners[j];
          curTileSettings->y_corners[j]=y_corners[j];
        }
        //Some safety checks when odd files come out of the projection algorithm
        if((x_corners[0]>=DBL_MAX||x_corners[0]<=-DBL_MAX)&&x_div==1&&x_div==1){
          curTileSettings->x_corners[0]=drawImage->Geo->dfBBOX[2];
          curTileSettings->x_corners[1]=drawImage->Geo->dfBBOX[2];
          curTileSettings->x_corners[2]=drawImage->Geo->dfBBOX[0];
          curTileSettings->x_corners[3]=drawImage->Geo->dfBBOX[0];
          curTileSettings->y_corners[0]=drawImage->Geo->dfBBOX[3];
          curTileSettings->y_corners[1]=drawImage->Geo->dfBBOX[1];
          curTileSettings->y_corners[2]=drawImage->Geo->dfBBOX[1];
          curTileSettings->y_corners[3]=drawImage->Geo->dfBBOX[3];
        }
        curTileSettings->tile_offset_x=int(double(x)*tile_width);
        curTileSettings->tile_offset_y=int(double(y)*tile_height);
        curTileSettings->drawTile=drawTileClass;
      }
    }

    if(useThreading==false){
        DrawMultipleTileSettings dmf;
        dmf.ct=drawTileSettings;
        dmf.numberOfTiles=numberOfTiles;
        dmf.startTile=0;
        dmf.endTile=numberOfTiles;
        drawTiles(&dmf);
    }
    
    if(useThreading==true){
      int errcode;
      pthread_t threads[numThreads];  
      DrawMultipleTileSettings dmf[numThreads];
      int tileBlockSize=numberOfTiles/numThreads;
      //Divide the tiles over the threads, and start the thread.
      for(int j=0;j<numThreads;j++){
        dmf[j].ct=drawTileSettings;
        dmf[j].numberOfTiles=numberOfTiles;
        dmf[j].startTile=tileBlockSize*j;;
        dmf[j].endTile=tileBlockSize*(j+1);
        
        //Make sure that all blocks are processed
        if(j==numThreads-1)dmf[j].endTile=numberOfTiles;
        
        //CDBDebug("%d - start %d stop %d",j,dmf[j].startTile,dmf[j].endTile);
        DrawMultipleTileSettings *t_dmf=&dmf[j];
        errcode=pthread_create(&threads[j],NULL,drawTiles,t_dmf);
        if(errcode){CDBError("pthread_create");return ;}
      }
      // reap the threads as they exit 
      for (int worker=0; worker<numThreads; worker++) {
        int *status;//TODO fix somehow the return status of the threads.
        // wait for thread to terminate 
        errcode=pthread_join(threads[worker],(void **) &status);
        if(errcode) { CDBError("pthread_join");return;}
      }
    }
    delete[] drawTileSettings;
    delete drawTileClass;
  }
};
#endif
