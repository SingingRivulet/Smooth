#include "terrain.h"
#include "mempool.h"
#include <stdio.h>
#include <set>
namespace smoothly{

typedef mempool<terrain::chunk> chunkpool;
typedef mempool<terrain::item> itempool;
terrain::terrain(){
    this->cpool=new chunkpool;
    this->ipool=new itempool;
    this->texture=NULL;
    for(int i=0;i<7;i++){
        for(int j=0;j<7;j++){
            visualChunk[i][j]=NULL;
        }
    }
}

terrain::~terrain(){
    delete (chunkpool*)this->cpool;
    delete (itempool*)this->ipool;
}
void terrain::destroy(){
    for(int i=0;i<7;i++){
        for(int j=0;j<7;j++){
            if(visualChunk[i][j]){
                visualChunk[i][j]->remove();
                removecChunk(visualChunk[i][j]);
            }
        }
    }
}

terrain::item * terrain::createItem(){
    auto ptr=((itempool*)this->ipool)->get();
    
    return ptr;
}

void terrain::destroyItem(terrain::item * ptr){
    ((itempool*)this->ipool)->del(ptr);
}

terrain::chunk * terrain::createChunk(){
    auto ptr=((chunkpool*)this->cpool)->get();
    ptr->scene=this->scene;
    ptr->device=this->device;
    ptr->T=new float*[pointNum];
    for(int i=0;i<pointNum;i++){
        ptr->T[i]=new float[pointNum];
    }
    ptr->parent=this;
    ptr->generator=&(this->generator);
    return ptr;
}

void terrain::removecChunk(terrain::chunk * ptr){
    for(int i=0;i<pointNum;i++){
        delete [] ptr->T[i];
    }
    delete [] ptr->T;
    ((chunkpool*)this->cpool)->del(ptr);
}

void terrain::visualChunkUpdate(irr::s32 x , irr::s32 y , bool force){
    if(x==px && y==py && !force)
        return;
    int i,j;
    std::map<ipair,chunk*> rm;
    
    for(auto it : chunks){
        rm[it.first]=it.second;
    }
    
    for(i=0;i<7;i++){
        for(j=0;j<7;j++){
        
            int ix=x+i-3;
            int iy=y+j-3;
            ipair posi(ix , iy);
            auto it2=chunks.find(posi);
            if(it2!=chunks.end()){
                rm.erase(posi);
                visualChunk[i][j]=it2->second;
            }else{
                auto ptr=createChunk();
                //2号是中心
                updateChunk(ptr , ix , iy);
                visualChunk[i][j]=ptr;
                chunks[posi]=ptr;
            }
        }
    }
    
    for(auto it3:rm){
        it3.second->remove();
        removecChunk(it3.second);
    }
}

float terrain::genTerrain(
    float ** img,  //高度图边长=chunk边长+1
    irr::s32 x , irr::s32 y //chunk坐标，真实坐标/32
){
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

float terrain::chunk::getRealHight(float x,float y){
    return parent->getRealHight(x,y);
}

bool terrain::itemExist(int ix,int iy,long iitemId,int imapId){
    
}

bool terrain::remove(const mapid & mid){
    auto it=allItems.find(mid);
    if(it==allItems.end()){
        return false;
    }
    
    it->second->inChunk->items.erase(it->second);
    it->second->remove();
    destroyItem(it->second);
    
    allItems.erase(it);
    return true;
}

int terrain::chunk::add(
    long id,
    const irr::core::vector3df & p,
    const irr::core::vector3df & r,
    const irr::core::vector3df & s
){
    auto mit=parent->m->items.find(id);
    if(mit==parent->m->items.end()){
        return -1;
    }
    int mapId=this->getId(id);
    //printf("add id=%d (%f,%f,%f) mapId=%d\n" , id , p.X , p.Y , p.Z , mapId);
    //set ptr
    auto ptr=parent->createItem();
    ptr->parent=mit->second;
    ptr->mesh=mit->second->mesh;
    ptr->inChunk=this;
    ptr->id=id;
    ptr->mapId=mapId;
    //set node
    ptr->node=parent->scene->addMeshSceneNode(
        ptr->mesh,
        0,-1,p,r,s
    );
    ptr->node->setMaterialFlag(irr::video::EMF_LIGHTING, false );
    //set index
    this->items.insert(ptr);
    parent->allItems[mapid(x,y,id,mapId)]=ptr;
    ///////
    return ptr->mapId;
}

void terrain::getItems(irr::s32 x , irr::s32 y , terrain::chunk * ch){
    for(auto it:m->mapGenFuncs){
        if(it)
            it(x,y,getTemperature(x,y),getHumidity(x,y),getHight(x,y),ch);
    }
}

void terrain::updateChunk(terrain::chunk * ch, irr::s32 x , irr::s32 y){
    auto driver=this->scene->getVideoDriver();
    auto max=genTerrain(ch->T , x ,y);
    printf("update chunk \tx=%d \t y=%d \t max=%f\n",x,y,max);
    
    float len=33.0f/(float)pointNum;
    ch->mesh=this->createTerrainMesh(
        this->texture ,
        ch->T , pointNum , pointNum ,
        irr::core::dimension2d<irr::f32>(len , len),
        driver,
        irr::core::dimension2d<irr::u32>(pointNum , pointNum),
        true
    );
    ch->node=scene->addMeshSceneNode(
        ch->mesh,
        0,-1
    );
    ch->selector=scene->createOctreeTriangleSelector(ch->mesh,ch->node);
    ch->node->setTriangleSelector(ch->selector);
    ch->node->setPosition(irr::core::vector3df(x*32.0f , 0 , y*32.0f));
    //printf("position:(%f,%f)(%d,%d)\n",x*32.0f,y*32.0f,x,y);
    ch->node->setMaterialFlag(irr::video::EMF_LIGHTING, false );
    ch->x=x;
    ch->y=y;
    ch->itemNum.clear();
    getItems(x,y,ch);
}

void terrain::genTexture(){
    if(this->texture)
        return;
    printf("generate texture\n");
    auto img=this->scene->getVideoDriver()->createImage(irr::video::ECF_R8G8B8 , irr::core::dimension2d<irr::u32>(64,64));
    perlin3d r;
    r.seed=1234;
    
    auto pitch=img->getPitch();
    auto data=(irr::u8*)(img->lock());
    
    for(int i=0;i<64;i++){
        for(int j=0;j<64;j++){
            irr::u8* dest = data + ( j * pitch ) + ( i * 3 );
            int v=r.get(i/10.0f , j/10.0f , 16);
            v%=256;
            dest[0]=v;
            dest[1]=v;
            dest[2]=v;
        }
    }
    img->unlock();
    this->texture=img;
}

void terrain::destroyTexture(){
    this->texture->drop();
    this->texture=NULL;
}
void terrain::getNearChunk(bool(*callback)(chunk*,void*),void * arg){
    for(int i=2;i<5;i++){
        for(int j=2;j<5;j++){
            if(visualChunk[i][j])
                if(!callback(visualChunk[i][j],arg))
                    return;
        }
    }
}
bool terrain::selectPoint(const irr::core::line3d<irr::f32>& ray,irr::core::vector3df& outCollisionPoint){
    irr::core::triangle3df  outTriangle;
    irr::scene::ISceneNode* outNode;
    for(int i=2;i<5;i++){
        for(int j=2;j<5;j++){
            if(visualChunk[i][j]){
                if(visualChunk[i][j]->getCollisionPoint(ray,outCollisionPoint,outTriangle,outNode))
                    return true;
            }
        }
    }
    return false;
}
}//namespace smoothly

