

#include "cashgen.h"
#include "FZoneGeneratorWorker.h"
#include "UnrealLibNoise.h"
#include "KismetProceduralMeshLibrary.h"

FZoneGeneratorWorker::FZoneGeneratorWorker(AZoneManager*		apZoneManager,
	FZoneConfig*			aZoneConfig,
	Point*				aOffSet,
	TMap<uint8, eLODStatus>*   aZoneJobData,
	uint8 aLOD,
	TArray<FVector>*	aVertices,
	TArray<int32>*		aTriangles,
	TArray<FVector>*	aNormals,
	TArray<FVector2D>*	aUV0,
	TArray<FColor>*		aVertexColors,
	TArray<FProcMeshTangent>* aTangents,
	TArray<FVector>* aHeightMap)
{
	pCallingZoneManager = apZoneManager;
	pOffset = aOffSet;
	pZoneJobData = aZoneJobData;
	MyLOD = aLOD;
	pZoneConfig = aZoneConfig;
	pVertices = aVertices;
	pTriangles = aTriangles;
	pNormals = aNormals;
	pUV0 = aUV0;
	pVertexColors = aVertexColors;
	pTangents = aTangents;
	pHeightMap = aHeightMap;

}

FZoneGeneratorWorker::~FZoneGeneratorWorker()
{

}

bool FZoneGeneratorWorker::Init()
{
	return true;
}

uint32 FZoneGeneratorWorker::Run()
{
	ProcessTerrainMap();

	ProcessGeometry();

	if (pCallingZoneManager->MyLODMeshStatus[MyLOD] == eLODStatus::BUILDING_REQUIRES_CREATE) {
		pCallingZoneManager->MyLODMeshStatus[MyLOD] = eLODStatus::READY_TO_DRAW_REQUIRES_CREATE;
	}
	else if(pCallingZoneManager->MyLODMeshStatus[MyLOD] == eLODStatus::BUILDING)
	{
		pCallingZoneManager->MyLODMeshStatus[MyLOD] = eLODStatus::READY_TO_DRAW;
	}


	return 1;
}

void FZoneGeneratorWorker::Stop()
{

}

void FZoneGeneratorWorker::Exit()
{

}

void FZoneGeneratorWorker::ProcessTerrainMap()
{
	MyMaxHeight = 0.0f;

	int32 exX = MyLOD == 0 ? pZoneConfig->XUnits + 3 : (pZoneConfig->XUnits / (FMath::Pow(2, MyLOD))) + 3;
	int32 exY = MyLOD == 0 ? pZoneConfig->YUnits + 3 : (pZoneConfig->YUnits / (FMath::Pow(2, MyLOD))) + 3;

	int32 exUnitSize = MyLOD == 0 ? pZoneConfig->UnitSize : pZoneConfig->UnitSize * (FMath::Pow(2, MyLOD));

	// Calculate the new noisemap
	for (int x = 0; x < exX; ++x)
	{
		for (int y = 0; y < exY; ++y)
		{
			int32 worldX = (((pOffset->x * (exX - 3) + x)) * exUnitSize) - exUnitSize;
			int32 worldY = (((pOffset->y * (exY - 3) + y)) * exUnitSize) - exUnitSize;
			(*pHeightMap)[x + (exX*y)] = FVector(x* exUnitSize, y*exUnitSize, pZoneConfig->noiseModule->GetValue(worldX, worldY, 0.0f) * pZoneConfig->Amplitude);
		}
	}
}

// Builds the geometry for the mesh
void FZoneGeneratorWorker::ProcessGeometry()
{
	int32 vertCounter = 0;
	int32 triCounter = 0;

	int32 xUnits = MyLOD == 0 ? pZoneConfig->XUnits : (pZoneConfig->XUnits / (FMath::Pow(2, MyLOD)));
	int32 yUnits = MyLOD == 0 ? pZoneConfig->YUnits : (pZoneConfig->YUnits / (FMath::Pow(2, MyLOD)));

	// Generate the mesh data for each block
	for (int32 y = 0; y < yUnits; ++y)
	{
		for (int32 x = 0; x < xUnits; ++x)
		{
			UpdateOneBlockGeometry(x, y, vertCounter, triCounter);
		}
	}
}

// Updates the vertices and other data for a single block (two tris)
void FZoneGeneratorWorker::UpdateOneBlockGeometry(const int aX, const int aY, int32& aVertCounter, int32& triCounter)
{
	int32 thisX = aX;
	int32 thisY = aY;
	int32 heightMapX = thisX + 1;
	int32 heightMapY = thisY + 1;

	int32 rowLength = MyLOD == 0 ? pZoneConfig->XUnits + 1 : (pZoneConfig->XUnits / (FMath::Pow(2, MyLOD)) + 1);
	int32 heightMapRowLength = rowLength + 2;

	int32 exUnitSize = MyLOD == 0 ? pZoneConfig->UnitSize : pZoneConfig->UnitSize * (FMath::Pow(2, MyLOD));

	FVector heightMapToWorldOffset = FVector(exUnitSize, exUnitSize, 0.0f);

	// BR
	(*pVertices)[thisX + (thisY * rowLength)]				= (*pHeightMap)[heightMapX + (heightMapY * heightMapRowLength)] - heightMapToWorldOffset;
	// TR
	(*pVertices)[thisX + ((thisY + 1) * rowLength)]			= (*pHeightMap)[heightMapX + ((heightMapY + 1) * heightMapRowLength)] - heightMapToWorldOffset;
	// BL
	(*pVertices)[(thisX + 1) + (thisY * rowLength)]			= (*pHeightMap)[(heightMapX + 1) + (heightMapY * heightMapRowLength)] - heightMapToWorldOffset;
	// BR
	(*pVertices)[(thisX + 1) + ((thisY + 1) * rowLength)]	= (*pHeightMap)[(heightMapX + 1) + ((heightMapY + 1) * heightMapRowLength)] - heightMapToWorldOffset;

	(*pNormals)[thisX + (thisY * rowLength)]				= GetNormalFromHeightMapForVertex(thisX, thisY);
	(*pNormals)[thisX + ((thisY + 1) * rowLength)]			= GetNormalFromHeightMapForVertex(thisX, thisY + 1);
	(*pNormals)[(thisX + 1) + (thisY * rowLength)]			= GetNormalFromHeightMapForVertex(thisX + 1, thisY);
	(*pNormals)[(thisX + 1) + ((thisY + 1) * rowLength)]	= GetNormalFromHeightMapForVertex(thisX + 1, thisY + 1);

	(*pTangents)[thisX + (thisY * rowLength)] = GetTangentFromNormal((*pNormals)[thisX + (thisY * rowLength)]);
	(*pTangents)[thisX + ((thisY + 1) * rowLength)] = GetTangentFromNormal((*pNormals)[thisX + ((thisY + 1) * rowLength)]);
	(*pTangents)[(thisX + 1) + (thisY * rowLength)] = GetTangentFromNormal((*pNormals)[(thisX + 1) + (thisY * rowLength)]);
	(*pTangents)[(thisX + 1) + ((thisY + 1) * rowLength)] = GetTangentFromNormal((*pNormals)[(thisX + 1) + ((thisY + 1) * rowLength)]);

	//TODO: This isn't doing anything at the moment 
	(*pVertexColors)[thisX + (thisY * rowLength)].R = (255 / 50000.0f);
	(*pVertexColors)[thisX + ((thisY + 1) * rowLength)].R = (255 / 50000.0f);
	(*pVertexColors)[(thisX + 1) + (thisY * rowLength)].R = (255 / 50000.0f);
	(*pVertexColors)[(thisX + 1) + ((thisY + 1) * rowLength)].R = (255 / 50000.0f);

}

// I won't even pretend to know what this is doing
FProcMeshTangent FZoneGeneratorWorker::GetTangentFromNormal(const FVector aNormal)
{
	FVector tangentVec, bitangentVec;
	FVector c1, c2;

	c1 = FVector::CrossProduct(aNormal, FVector(0.0f, 0.0f, 1.0f));
	c2 = FVector::CrossProduct(aNormal, FVector(0.0f, 1.0f, 0.0f));

	if (c1.Size() > c2.Size())
	{
		tangentVec = c1;
	}
	else
	{
		tangentVec = c2;
	}

	tangentVec = tangentVec.GetSafeNormal();
	bitangentVec = FVector::CrossProduct(aNormal, tangentVec);


	return FProcMeshTangent(bitangentVec, false );
}

FVector FZoneGeneratorWorker::GetNormalFromHeightMapForVertex(const int32 vertexX, const int32 vertexY)
{
	FVector result;

	int32 rowLength = MyLOD == 0 ? pZoneConfig->XUnits + 1 : (pZoneConfig->XUnits / (FMath::Pow(2, MyLOD)) + 1);
	int32 heightMapRowLength = rowLength + 2;

	// the heightmapIndex for this vertex index
	int32 heightMapIndex = vertexX + 1 + ((vertexY + 1) * heightMapRowLength);

	// Get the 8 neighbouring points
	FVector up, down, left, right, upleft, upright, downleft, downright;

	up		= (*pHeightMap)[heightMapIndex + heightMapRowLength] - (*pHeightMap)[heightMapIndex];
	down	= (*pHeightMap)[heightMapIndex - heightMapRowLength] - (*pHeightMap)[heightMapIndex];
	left	= (*pHeightMap)[heightMapIndex + 1] - (*pHeightMap)[heightMapIndex];
	right	= (*pHeightMap)[heightMapIndex - 1] - (*pHeightMap)[heightMapIndex];
	upleft = (*pHeightMap)[heightMapIndex + 1 + heightMapRowLength] - (*pHeightMap)[heightMapIndex];
	upright = (*pHeightMap)[heightMapIndex - 1 + heightMapRowLength] - (*pHeightMap)[heightMapIndex];
	downleft = (*pHeightMap)[heightMapIndex + 1 - heightMapRowLength] - (*pHeightMap)[heightMapIndex];
	downright = (*pHeightMap)[heightMapIndex - 1 - heightMapRowLength] - (*pHeightMap)[heightMapIndex];

	// Now the eight normals from the triangles this forms
	FVector n1, n2, n3, n4, n5, n6, n7, n8;

	n1 = FVector::CrossProduct(left, upleft);
	n2 = FVector::CrossProduct(upleft, up);
	n3 = FVector::CrossProduct(up, upright);
	n4 = FVector::CrossProduct(upright, right);
	n5 = FVector::CrossProduct(right, downright);
	n6 = FVector::CrossProduct(downright, down);
	n7 = FVector::CrossProduct(down, downleft);
	n8 = FVector::CrossProduct(downleft, left);

	
	result = n1 + n2 + n3 + n4 + n5 + n6 + n7 + n8;

	return result.GetSafeNormal();
}
