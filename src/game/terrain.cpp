#include "terrain.h"
namespace smoothly{

terrain::terrain(){
    setSeed(1234);
    mapBuf=new float*[33];
    for(auto i=0;i<33;i++)
        mapBuf[i]=new float[33];
}
terrain::~terrain(){
    for(auto it:chunks)
        freeChunk(it.second);
    
    for(auto i=0;i<33;i++)
        delete [] mapBuf[i];
    delete [] mapBuf;
}
float terrain::genTerrain(float ** img,int x , int y ,int pointNum){
    float h;
    float ix,iy;
    float max=0;
    float mx=0,my=0;
    float begX=x*32;
    float begY=y*32;
    float len=33.0f/((float)pointNum);
    
    for(int i=0;i<pointNum;i++){
        for(int j=0;j<pointNum;j++){
            //ix=len*(pointNum-i+1)+begX;
            ix=len*i+begX;
            //iy=len*(pointNum-j+1)+begY;
            iy=len*j+begY;
            h=getRealHight(ix,iy);
            if(h>max){
                max=h;
                mx=ix;
                my=iy;
            }
            img[i][j]=h;
        }
    }
    //printf("max:(%f,%f) begin:(%d,%d)\n",mx,my,begX,begY);
    return max;
}
terrain::chunk * terrain::genChunk(int x,int y){
    auto res = new chunk;
    genTerrain(mapBuf , x , y , 33);
    auto mesh=this->createTerrainMesh(
        NULL , 
        mapBuf , 33 , 33 ,
        irr::core::dimension2d<irr::f32>(1 , 1),
        irr::core::dimension2d<irr::u32>(33 , 33),
        true
    );
    res->node=scene->addMeshSceneNode(mesh,0,-1);
    res->node->setPosition(irr::core::vector3df(x*32.0f , 0 , y*32.0f));
    
    auto selector=scene->createOctreeTriangleSelector(mesh,res->node);   //创建选择器
    res->node->setTriangleSelector(selector);
    selector->drop();
    
    res->node->setMaterialFlag(irr::video::EMF_LIGHTING, true );
    res->node->addShadowVolumeSceneNode();
    
    if(showing.find(ipair(x,y))==showing.end())
        res->node->setVisible(false);
    
    res->node->updateAbsolutePosition();
    res->bodyState =setMotionState(res->node->getAbsoluteTransformation().pointer());
    
    res->rigidBody =createBody(res->bodyShape,res->bodyState);
    res->rigidBody->setCollisionFlags(btCollisionObject::CF_STATIC_OBJECT);
    res->rigidBody->setFriction(0.7);
    res->rigidBody->setRestitution(0.1);
    
    res->info.type=BODY_TERRAIN;
    res->info.ptr=res;
    res->rigidBody->setUserPointer(&(res->info));
    
    this->dynamicsWorld->addRigidBody(res->rigidBody);
    
    mesh->drop();
    return res;
}
void terrain::freeChunk(terrain::chunk * p){
    dynamicsWorld->removeRigidBody(p->rigidBody);
    delete p->rigidBody;
    delete p->bodyState;
    delete p->bodyShape;
    delete p->bodyMesh;
    p->node->remove();
    delete p;
}

#define findChunk(x,y) \
    auto it = chunks.find(ipair(x,y)); \
    if(it!=chunks.end())

void terrain::createChunk(int x,int y){
    findChunk(x,y){
        
    }else{
        chunks[ipair(x,y)] = genChunk(x,y);
    }
}
void terrain::showChunk(int x,int y){
    findChunk(x,y){
        it->second->show();
    }
    showing.insert(ipair(x,y));
}
void terrain::hideChunk(int x,int y){
    findChunk(x,y){
        it->second->hide();
    }
    showing.erase(ipair(x,y));
}
void terrain::releaseChunk(int x,int y){
    findChunk(x,y){
        freeChunk(it->second);
        chunks.erase(it);
    }
}

}