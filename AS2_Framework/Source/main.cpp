#include "../Externals/Include/Common.h"
#include "../VC14/fbxloader.h"

#include <ctime>
#include <cstdio>
#include <cstdlib>
#include <IL/il.h>
#include <vector>
#include <algorithm>
#include <fbxsdk.h>
#include <vector>
#include <string>



typedef struct _fbx_handles
{
	_fbx_handles()
	{
		lSdkManager = NULL;
		lScene = NULL;
	}

	FbxManager* lSdkManager;
	FbxScene* lScene;
	FbxArray<FbxString*> lAnimStackNameArray;
} fbx_handles;

void GetFbxAnimation(fbx_handles &handles, std::vector<tinyobj::shape_t> &shapes, float frame);
bool LoadFbx(fbx_handles &handles, std::vector<tinyobj::shape_t> &shapes, std::vector<tinyobj::material_t> &materials, std::string err, const char* fbxFile);
void ReleaseFbx(fbx_handles &handles);

#define MENU_TIMER_START 1
#define MENU_TIMER_STOP 2
#define MENU_EXIT 3
#define SHADOW_WIDTH 8192
#define SHADOW_HEIGHT 8192

GLubyte timer_cnt = 0;
bool timer_enabled = true;
unsigned int timer_speed = 16;

using namespace glm;
using namespace std;
using namespace tinyobj;

#ifdef IOS_REF
#undef  IOS_REF
#define IOS_REF (*(pManager->GetIOSettings()))
#endif

typedef struct
{
	std::vector<int> vertexControlIndices; // translate gl_VertexID to control point index
	std::vector<int> vertexJointIndices; // 4 joints for one vertex
	std::vector<float> vertexJointWeights; // 4 joint weights for one vertex
	std::vector<std::vector<float> > jointTransformMatrices; // 16 float column major matrix for each joint for each frame
	int num_frames;
} animation_t;

vector<shape_t> gShapes;
vector<material_t> gMaterials;
vector<animation_t> gAnimations;
vector<shape_t> gShapeAnim;

bool LoadScene(FbxManager* pManager, FbxScene* pScene, FbxArray<FbxString*> pAnimStackNameArray, const char* pFilename);
void LoadCacheRecursive(FbxScene* pScene, FbxNode * pNode, FbxAMatrix& pParentGlobalPosition, FbxTime& pTime);
void LoadAnimationRecursive(FbxScene* pScene, FbxNode * pNode, FbxAMatrix& pParentGlobalPosition, FbxTime& pTime);
void InitializeSdkObjects(FbxManager*& pManager, FbxScene*& pScene);
void DestroySdkObjects(FbxManager* pManager, bool pExitStatus);

void DisplayMetaData(FbxScene* pScene);
void DisplayHierarchy(FbxScene* pScene);
void DisplayHierarchy(FbxNode* pNode, int pDepth);

FbxAMatrix GetGlobalPosition(FbxNode* pNode, const FbxTime& pTime, FbxPose* pPose = NULL, FbxAMatrix* pParentGlobalPosition = NULL);
FbxAMatrix GetPoseMatrix(FbxPose* pPose, int pNodeIndex);
FbxAMatrix GetGeometry(FbxNode* pNode);

void MatrixScale(FbxAMatrix& pMatrix, double pValue);
void MatrixAddToDiagonal(FbxAMatrix& pMatrix, double pValue);
void MatrixAdd(FbxAMatrix& pDstMatrix, FbxAMatrix& pSrcMatrix);

void ComputeSkinDeformation(FbxAMatrix& pGlobalPosition, FbxMesh* pMesh, FbxTime& pTime, FbxVector4* pVertexArray, FbxPose* pPose);
void ComputeShapeDeformation(FbxMesh* pMesh, FbxTime& pTime, FbxAnimLayer * pAnimLayer, FbxVector4* pVertexArray);
void ComputeClusterDeformation(FbxAMatrix& pGlobalPosition, FbxMesh* pMesh, FbxCluster* pCluster, FbxAMatrix& pVertexTransformMatrix, FbxTime pTime, FbxPose* pPose);

void GetFbxAnimation(fbx_handles &handles, std::vector<tinyobj::shape_t> &shapes, float frame)
{
	if (handles.lScene != 0)
	{
		frame = std::min(std::max(frame, 0.0f), 1.0f);
		FbxTimeSpan lTimeLineTimeSpan;
		handles.lScene->GetGlobalSettings().GetTimelineDefaultTimeSpan(lTimeLineTimeSpan);
		FbxTime lTime = lTimeLineTimeSpan.GetStart() + ((lTimeLineTimeSpan.GetStop() - lTimeLineTimeSpan.GetStart()) / 10000) * (10000 * frame);
		gShapeAnim.clear();
		FbxAMatrix lDummyGlobalPosition;
		LoadAnimationRecursive(handles.lScene, handles.lScene->GetRootNode(), lDummyGlobalPosition, lTime);
		shapes = gShapeAnim;
	}
}

bool LoadFbx(fbx_handles &handles, vector<shape_t> &shapes, vector<material_t> &materials, std::string err, const char* pFileName)
{
	gShapes.clear();
	gMaterials.clear();
	gAnimations.clear();

	bool lResult;
	InitializeSdkObjects(handles.lSdkManager, handles.lScene);
	lResult = LoadScene(handles.lSdkManager, handles.lScene, handles.lAnimStackNameArray, pFileName);

	if (lResult == false)
	{
		FBXSDK_printf("\n\nAn error occurred while loading the scene...");
		DestroySdkObjects(handles.lSdkManager, lResult);
		return false;
	}
	else
	{
		// Display the scene.
		// DisplayMetaData(handles.lScene);
		// FBXSDK_printf("\n\n---------\nHierarchy\n---------\n\n");
		// DisplayHierarchy(handles.lScene);

		// Load data.
		FbxAMatrix lDummyGlobalPosition;
		FbxTimeSpan lTimeLineTimeSpan;
		handles.lScene->GetGlobalSettings().GetTimelineDefaultTimeSpan(lTimeLineTimeSpan);
		FbxTime lTime = lTimeLineTimeSpan.GetStart();
		LoadCacheRecursive(handles.lScene, handles.lScene->GetRootNode(), lDummyGlobalPosition, lTime);
	}

	shapes = gShapes;
	materials = gMaterials;
	return true;
}

void ReleaseFbx(fbx_handles &handles)
{
	if (handles.lScene)
	{
		bool lResult;
		DestroySdkObjects(handles.lSdkManager, lResult);
		FbxArrayDelete(handles.lAnimStackNameArray);
		handles.lSdkManager = 0;
		handles.lScene = 0;
	}
}

void DisplayHierarchy(FbxScene* pScene)
{
	int i;
	FbxNode* lRootNode = pScene->GetRootNode();

	for (i = 0; i < lRootNode->GetChildCount(); i++)
	{
		DisplayHierarchy(lRootNode->GetChild(i), 0);
	}
}

void DisplayHierarchy(FbxNode* pNode, int pDepth)
{
	FbxString lString;
	int i;

	for (i = 0; i < pDepth; i++)
	{
		lString += "     ";
	}

	lString += pNode->GetName();
	lString += "\n";

	FBXSDK_printf(lString.Buffer());

	for (i = 0; i < pNode->GetChildCount(); i++)
	{
		DisplayHierarchy(pNode->GetChild(i), pDepth + 1);
	}
}

void InitializeSdkObjects(FbxManager*& pManager, FbxScene*& pScene)
{
	//The first thing to do is to create the FBX Manager which is the object allocator for almost all the classes in the SDK
	pManager = FbxManager::Create();
	if (!pManager)
	{
		FBXSDK_printf("Error: Unable to create FBX Manager!\n");
		exit(1);
	}
	else FBXSDK_printf("Autodesk FBX SDK version %s\n", pManager->GetVersion());

	//Create an IOSettings object. This object holds all import/export settings.
	FbxIOSettings* ios = FbxIOSettings::Create(pManager, IOSROOT);
	pManager->SetIOSettings(ios);

	//Load plugins from the executable directory (optional)
	FbxString lPath = FbxGetApplicationDirectory();
	pManager->LoadPluginsDirectory(lPath.Buffer());

	//Create an FBX scene. This object holds most objects imported/exported from/to files.
	pScene = FbxScene::Create(pManager, "My Scene");
	if (!pScene)
	{
		FBXSDK_printf("Error: Unable to create FBX scene!\n");
		exit(1);
	}
}

bool LoadScene(FbxManager* pManager, FbxScene* pScene, FbxArray<FbxString*> pAnimStackNameArray, const char* pFilename)
{
	int lFileMajor, lFileMinor, lFileRevision;
	int lSDKMajor, lSDKMinor, lSDKRevision;
	int i, lAnimStackCount;
	bool lStatus;
	char lPassword[1024];

	// Get the file version number generate by the FBX SDK.
	FbxManager::GetFileFormatVersion(lSDKMajor, lSDKMinor, lSDKRevision);

	// Create an importer.
	FbxImporter* lImporter = FbxImporter::Create(pManager, "");

	// Initialize the importer by providing a filename.
	const bool lImportStatus = lImporter->Initialize(pFilename, -1, pManager->GetIOSettings());
	lImporter->GetFileVersion(lFileMajor, lFileMinor, lFileRevision);

	if (!lImportStatus)
	{
		FbxString error = lImporter->GetStatus().GetErrorString();
		FBXSDK_printf("Call to FbxImporter::Initialize() failed.\n");
		FBXSDK_printf("Error returned: %s\n\n", error.Buffer());

		if (lImporter->GetStatus().GetCode() == FbxStatus::eInvalidFileVersion)
		{
			FBXSDK_printf("FBX file format version for this FBX SDK is %d.%d.%d\n", lSDKMajor, lSDKMinor, lSDKRevision);
			FBXSDK_printf("FBX file format version for file '%s' is %d.%d.%d\n\n", pFilename, lFileMajor, lFileMinor, lFileRevision);
		}

		return false;
	}

	FBXSDK_printf("FBX file format version for this FBX SDK is %d.%d.%d\n", lSDKMajor, lSDKMinor, lSDKRevision);

	if (lImporter->IsFBX())
	{
		FBXSDK_printf("FBX file format version for file '%s' is %d.%d.%d\n\n", pFilename, lFileMajor, lFileMinor, lFileRevision);

		// From this point, it is possible to access animation stack information without
		// the expense of loading the entire file.

		FBXSDK_printf("Animation Stack Information\n");

		lAnimStackCount = lImporter->GetAnimStackCount();

		FBXSDK_printf("    Number of Animation Stacks: %d\n", lAnimStackCount);
		FBXSDK_printf("    Current Animation Stack: \"%s\"\n", lImporter->GetActiveAnimStackName().Buffer());
		FBXSDK_printf("\n");

		for (i = 0; i < lAnimStackCount; i++)
		{
			FbxTakeInfo* lTakeInfo = lImporter->GetTakeInfo(i);

			FBXSDK_printf("    Animation Stack %d\n", i);
			FBXSDK_printf("         Name: \"%s\"\n", lTakeInfo->mName.Buffer());
			FBXSDK_printf("         Description: \"%s\"\n", lTakeInfo->mDescription.Buffer());

			// Change the value of the import name if the animation stack should be imported 
			// under a different name.
			FBXSDK_printf("         Import Name: \"%s\"\n", lTakeInfo->mImportName.Buffer());

			// Set the value of the import state to false if the animation stack should be not
			// be imported. 
			FBXSDK_printf("         Import State: %s\n", lTakeInfo->mSelect ? "true" : "false");
			FBXSDK_printf("\n");
		}
	}

	// Import the scene.
	lStatus = lImporter->Import(pScene);

	if (lStatus == false && lImporter->GetStatus().GetCode() == FbxStatus::ePasswordError)
	{
		FBXSDK_printf("Please enter password: ");

		lPassword[0] = '\0';

		FBXSDK_CRT_SECURE_NO_WARNING_BEGIN
			scanf("%s", lPassword);
		FBXSDK_CRT_SECURE_NO_WARNING_END

			FbxString lString(lPassword);

		IOS_REF.SetStringProp(IMP_FBX_PASSWORD, lString);
		IOS_REF.SetBoolProp(IMP_FBX_PASSWORD_ENABLE, true);

		lStatus = lImporter->Import(pScene);

		if (lStatus == false && lImporter->GetStatus().GetCode() == FbxStatus::ePasswordError)
		{
			FBXSDK_printf("\nPassword is wrong, import aborted.\n");
		}
	}

	if (lStatus)
	{
		// Convert Axis System to up = Y Axis, Right-Handed Coordinate (OpenGL Style)
		FbxAxisSystem SceneAxisSystem = pScene->GetGlobalSettings().GetAxisSystem();
		FbxAxisSystem OurAxisSystem(FbxAxisSystem::eYAxis, FbxAxisSystem::eParityOdd, FbxAxisSystem::eRightHanded);
		if (SceneAxisSystem != OurAxisSystem)
		{
			OurAxisSystem.ConvertScene(pScene);
		}

		// Convert Unit System to what is used in this example, if needed
		FbxSystemUnit SceneSystemUnit = pScene->GetGlobalSettings().GetSystemUnit();
		if (SceneSystemUnit.GetScaleFactor() != 100.0)
		{
			//The unit in this example is centimeter.
			FbxSystemUnit::m.ConvertScene(pScene);
		}

		// Get the list of all the animation stack.
		pScene->FillAnimStackNameArray(pAnimStackNameArray);

		// Convert mesh, NURBS and patch into triangle mesh
		FbxGeometryConverter lGeomConverter(pManager);
		lGeomConverter.Triangulate(pScene, true);
	}

	// Destroy the importer.
	lImporter->Destroy();

	return lStatus;
}

void DestroySdkObjects(FbxManager* pManager, bool pExitStatus)
{
	//Delete the FBX Manager. All the objects that have been allocated using the FBX Manager and that haven't been explicitly destroyed are also automatically destroyed.
	if (pManager) pManager->Destroy();
	if (pExitStatus) FBXSDK_printf("Program Success!\n");
}

void DisplayMetaData(FbxScene* pScene)
{
	FbxDocumentInfo* sceneInfo = pScene->GetSceneInfo();
	if (sceneInfo)
	{
		FBXSDK_printf("\n\n--------------------\nMeta-Data\n--------------------\n\n");
		FBXSDK_printf("    Title: %s\n", sceneInfo->mTitle.Buffer());
		FBXSDK_printf("    Subject: %s\n", sceneInfo->mSubject.Buffer());
		FBXSDK_printf("    Author: %s\n", sceneInfo->mAuthor.Buffer());
		FBXSDK_printf("    Keywords: %s\n", sceneInfo->mKeywords.Buffer());
		FBXSDK_printf("    Revision: %s\n", sceneInfo->mRevision.Buffer());
		FBXSDK_printf("    Comment: %s\n", sceneInfo->mComment.Buffer());

		FbxThumbnail* thumbnail = sceneInfo->GetSceneThumbnail();
		if (thumbnail)
		{
			FBXSDK_printf("    Thumbnail:\n");

			switch (thumbnail->GetDataFormat())
			{
			case FbxThumbnail::eRGB_24:
				FBXSDK_printf("        Format: RGB\n");
				break;
			case FbxThumbnail::eRGBA_32:
				FBXSDK_printf("        Format: RGBA\n");
				break;
			}

			switch (thumbnail->GetSize())
			{
			default:
				break;
			case FbxThumbnail::eNotSet:
				FBXSDK_printf("        Size: no dimensions specified (%ld bytes)\n", thumbnail->GetSizeInBytes());
				break;
			case FbxThumbnail::e64x64:
				FBXSDK_printf("        Size: 64 x 64 pixels (%ld bytes)\n", thumbnail->GetSizeInBytes());
				break;
			case FbxThumbnail::e128x128:
				FBXSDK_printf("        Size: 128 x 128 pixels (%ld bytes)\n", thumbnail->GetSizeInBytes());
			}
		}
	}
}

// Get specific property value and connected texture if any.
// Value = Property value * Factor property value (if no factor property, multiply by 1).
FbxDouble3 GetMaterialProperty(const FbxSurfaceMaterial * pMaterial,
	const char * pPropertyName,
	const char * pFactorPropertyName,
	string & pTextureName)
{
	FbxDouble3 lResult(0, 0, 0);
	const FbxProperty lProperty = pMaterial->FindProperty(pPropertyName);
	const FbxProperty lFactorProperty = pMaterial->FindProperty(pFactorPropertyName);
	if (lProperty.IsValid() && lFactorProperty.IsValid())
	{
		lResult = lProperty.Get<FbxDouble3>();
		double lFactor = lFactorProperty.Get<FbxDouble>();
		if (lFactor != 1)
		{
			lResult[0] *= lFactor;
			lResult[1] *= lFactor;
			lResult[2] *= lFactor;
		}
	}

	if (lProperty.IsValid())
	{
		const int lTextureCount = lProperty.GetSrcObjectCount<FbxFileTexture>();
		if (lTextureCount)
		{
			const FbxFileTexture* lTexture = lProperty.GetSrcObject<FbxFileTexture>();
			if (lTexture)
			{
				pTextureName = lTexture->GetFileName();
			}
		}
	}

	return lResult;
}

void LoadAnimationRecursive(FbxScene* pScene, FbxNode *pNode, FbxAMatrix& pParentGlobalPosition, FbxTime& pTime)
{
	FbxAMatrix lGlobalPosition = GetGlobalPosition(pNode, pTime, 0, &pParentGlobalPosition);
	const int lMaterialCount = pNode->GetMaterialCount();
	FbxNodeAttribute* lNodeAttribute = pNode->GetNodeAttribute();
	if (lNodeAttribute)
	{
		// Bake mesh as VBO(vertex buffer object) into GPU.
		if (lNodeAttribute->GetAttributeType() == FbxNodeAttribute::eMesh)
		{
			FbxMesh * lMesh = pNode->GetMesh();
			if (lMesh)
			{
				const int lPolygonCount = lMesh->GetPolygonCount();
				bool lAllByControlPoint = true; // => true: glDrawElements / false: glDrawArrays

												// Count the polygon count of each material
				FbxLayerElementArrayTemplate<int>* lMaterialIndice = NULL;
				FbxGeometryElement::EMappingMode lMaterialMappingMode = FbxGeometryElement::eNone;
				if (lMesh->GetElementMaterial())
				{
					lMaterialIndice = &lMesh->GetElementMaterial()->GetIndexArray();
					lMaterialMappingMode = lMesh->GetElementMaterial()->GetMappingMode();
				}

				// Congregate all the data of a mesh to be cached in VBOs.
				// If normal or UV is by polygon vertex, record all vertex attributes by polygon vertex.
				bool lHasNormal = lMesh->GetElementNormalCount() > 0;
				bool lHasUV = lMesh->GetElementUVCount() > 0;
				FbxGeometryElement::EMappingMode lNormalMappingMode = FbxGeometryElement::eNone;
				FbxGeometryElement::EMappingMode lUVMappingMode = FbxGeometryElement::eNone;
				if (lHasNormal)
				{
					lNormalMappingMode = lMesh->GetElementNormal(0)->GetMappingMode();
					if (lNormalMappingMode == FbxGeometryElement::eNone)
					{
						lHasNormal = false;
					}
					if (lHasNormal && lNormalMappingMode != FbxGeometryElement::eByControlPoint)
					{
						lAllByControlPoint = false;
					}
				}
				if (lHasUV)
				{
					lUVMappingMode = lMesh->GetElementUV(0)->GetMappingMode();
					if (lUVMappingMode == FbxGeometryElement::eNone)
					{
						lHasUV = false;
					}
					if (lHasUV && lUVMappingMode != FbxGeometryElement::eByControlPoint)
					{
						lAllByControlPoint = false;
					}
				}

				// Allocate the array memory, by control point or by polygon vertex.
				int lPolygonVertexCount = lMesh->GetControlPointsCount();
				if (!lAllByControlPoint)
				{
					lPolygonVertexCount = lPolygonCount * 3;
				}
				vector<float> lVertices;
				lVertices.resize(lPolygonVertexCount * 3);
				vector<unsigned int> lIndices;
				lIndices.resize(lPolygonCount * 3);

				// Populate the array with vertex attribute, if by control point.
				FbxVector4 * lControlPoints = lMesh->GetControlPoints();
				/////////////////////////
				if ((FbxSkin *)lMesh->GetDeformer(0, FbxDeformer::eSkin) != 0)
				{
					lControlPoints = new FbxVector4[lMesh->GetControlPointsCount()];
					memcpy(lControlPoints, lMesh->GetControlPoints(), lMesh->GetControlPointsCount() * sizeof(FbxVector4));

					// select the base layer from the animation stack
					FbxAnimStack * lCurrentAnimationStack = pScene->GetSrcObject<FbxAnimStack>(0);
					// we assume that the first animation layer connected to the animation stack is the base layer
					// (this is the assumption made in the FBXSDK)
					FbxAnimLayer *mCurrentAnimLayer = lCurrentAnimationStack->GetMember<FbxAnimLayer>();

					// ComputeShapeDeformation(lMesh, pTime, mCurrentAnimLayer, lControlPoints);
					FbxAMatrix lGeometryOffset = GetGeometry(pNode);
					FbxAMatrix lGlobalOffPosition = lGlobalPosition * lGeometryOffset;
					ComputeSkinDeformation(lGlobalOffPosition, lMesh, pTime, lControlPoints, NULL);
				}
				/////////////////////////
				vector<int> vertexControlIndices;
				FbxVector4 lCurrentVertex;
				FbxVector4 lCurrentNormal;
				FbxVector2 lCurrentUV;
				if (lAllByControlPoint)
				{
					for (int lIndex = 0; lIndex < lPolygonVertexCount; ++lIndex)
					{
						// Save the vertex position.
						lCurrentVertex = lControlPoints[lIndex];
						lVertices[lIndex * 3] = static_cast<float>(lCurrentVertex[0]);
						lVertices[lIndex * 3 + 1] = static_cast<float>(lCurrentVertex[1]);
						lVertices[lIndex * 3 + 2] = static_cast<float>(lCurrentVertex[2]);
						vertexControlIndices.push_back(lIndex);
					}

				}

				int lVertexCount = 0;
				for (int lPolygonIndex = 0; lPolygonIndex < lPolygonCount; ++lPolygonIndex)
				{
					for (int lVerticeIndex = 0; lVerticeIndex < 3; ++lVerticeIndex)
					{
						const int lControlPointIndex = lMesh->GetPolygonVertex(lPolygonIndex, lVerticeIndex);

						if (lAllByControlPoint)
						{
							lIndices[lPolygonIndex * 3 + lVerticeIndex] = static_cast<unsigned int>(lControlPointIndex);
						}
						// Populate the array with vertex attribute, if by polygon vertex.
						else
						{
							lIndices[lPolygonIndex * 3 + lVerticeIndex] = static_cast<unsigned int>(lVertexCount);

							lCurrentVertex = lControlPoints[lControlPointIndex];
							lVertices[lVertexCount * 3] = static_cast<float>(lCurrentVertex[0]);
							lVertices[lVertexCount * 3 + 1] = static_cast<float>(lCurrentVertex[1]);
							lVertices[lVertexCount * 3 + 2] = static_cast<float>(lCurrentVertex[2]);
							vertexControlIndices.push_back(lControlPointIndex);
						}
						++lVertexCount;
					}
				}
				shape_t shape;
				shape.mesh.positions = lVertices;
				gShapeAnim.push_back(shape);

				if ((FbxSkin *)lMesh->GetDeformer(0, FbxDeformer::eSkin) != 0)
				{
					delete[] lControlPoints;
				}
			}
		}
	}

	const int lChildCount = pNode->GetChildCount();
	for (int lChildIndex = 0; lChildIndex < lChildCount; ++lChildIndex)
	{
		LoadAnimationRecursive(pScene, pNode->GetChild(lChildIndex), lGlobalPosition, pTime);
	}
}

void LoadMaterials(FbxNode *pNode)
{
	// Bake material and hook as user data.
	int lMaterialIndexBase = gMaterials.size();
	const int lMaterialCount = pNode->GetMaterialCount();
	for (int lMaterialIndex = 0; lMaterialIndex < lMaterialCount; ++lMaterialIndex)
	{
		FbxSurfaceMaterial * lMaterial = pNode->GetMaterial(lMaterialIndex);
		material_t material;
		if (lMaterial)
		{
			string lTextureNameTemp;
			const FbxDouble3 lAmbient = GetMaterialProperty(lMaterial,
				FbxSurfaceMaterial::sAmbient, FbxSurfaceMaterial::sAmbientFactor, lTextureNameTemp);
			material.ambient[0] = static_cast<GLfloat>(lAmbient[0]);
			material.ambient[1] = static_cast<GLfloat>(lAmbient[1]);
			material.ambient[2] = static_cast<GLfloat>(lAmbient[2]);
			material.ambient_texname = lTextureNameTemp;

			const FbxDouble3 lDiffuse = GetMaterialProperty(lMaterial,
				FbxSurfaceMaterial::sDiffuse, FbxSurfaceMaterial::sDiffuseFactor, lTextureNameTemp);
			material.diffuse[0] = static_cast<GLfloat>(lDiffuse[0]);
			material.diffuse[1] = static_cast<GLfloat>(lDiffuse[1]);
			material.diffuse[2] = static_cast<GLfloat>(lDiffuse[2]);
			material.diffuse_texname = lTextureNameTemp;

			const FbxDouble3 lSpecular = GetMaterialProperty(lMaterial,
				FbxSurfaceMaterial::sSpecular, FbxSurfaceMaterial::sSpecularFactor, lTextureNameTemp);
			material.specular[0] = static_cast<GLfloat>(lSpecular[0]);
			material.specular[1] = static_cast<GLfloat>(lSpecular[1]);
			material.specular[2] = static_cast<GLfloat>(lSpecular[2]);
			material.specular_texname = lTextureNameTemp;

			FbxProperty lShininessProperty = lMaterial->FindProperty(FbxSurfaceMaterial::sShininess);
			if (lShininessProperty.IsValid())
			{
				double lShininess = lShininessProperty.Get<FbxDouble>();
				material.shininess = static_cast<GLfloat>(lShininess);
			}
		}
		gMaterials.push_back(material);
	}
}

void LoadCacheRecursive(FbxScene* pScene, FbxNode * pNode, FbxAMatrix& pParentGlobalPosition, FbxTime& pTime)
{
	FbxAMatrix lGlobalPosition = GetGlobalPosition(pNode, pTime, 0, &pParentGlobalPosition);
	// Bake material and hook as user data.
	int lMaterialIndexBase = gMaterials.size();
	LoadMaterials(pNode);

	FbxNodeAttribute* lNodeAttribute = pNode->GetNodeAttribute();
	if (lNodeAttribute)
	{
		// Bake mesh as VBO(vertex buffer object) into GPU.
		if (lNodeAttribute->GetAttributeType() == FbxNodeAttribute::eMesh)
		{
			FbxMesh * lMesh = pNode->GetMesh();
			if (lMesh)
			{
				const int lPolygonCount = lMesh->GetPolygonCount();
				bool lAllByControlPoint = true; // => true: glDrawElements / false: glDrawArrays

				// Count the polygon count of each material
				FbxLayerElementArrayTemplate<int>* lMaterialIndice = NULL;
				FbxGeometryElement::EMappingMode lMaterialMappingMode = FbxGeometryElement::eNone;
				if (lMesh->GetElementMaterial())
				{
					lMaterialIndice = &lMesh->GetElementMaterial()->GetIndexArray();
					lMaterialMappingMode = lMesh->GetElementMaterial()->GetMappingMode();
				}

				// Congregate all the data of a mesh to be cached in VBOs.
				// If normal or UV is by polygon vertex, record all vertex attributes by polygon vertex.
				bool lHasNormal = lMesh->GetElementNormalCount() > 0;
				bool lHasUV = lMesh->GetElementUVCount() > 0;
				FbxGeometryElement::EMappingMode lNormalMappingMode = FbxGeometryElement::eNone;
				FbxGeometryElement::EMappingMode lUVMappingMode = FbxGeometryElement::eNone;
				if (lHasNormal)
				{
					lNormalMappingMode = lMesh->GetElementNormal(0)->GetMappingMode();
					if (lNormalMappingMode == FbxGeometryElement::eNone)
					{
						lHasNormal = false;
					}
					if (lHasNormal && lNormalMappingMode != FbxGeometryElement::eByControlPoint)
					{
						lAllByControlPoint = false;
					}
				}
				if (lHasUV)
				{
					lUVMappingMode = lMesh->GetElementUV(0)->GetMappingMode();
					if (lUVMappingMode == FbxGeometryElement::eNone)
					{
						lHasUV = false;
					}
					if (lHasUV && lUVMappingMode != FbxGeometryElement::eByControlPoint)
					{
						lAllByControlPoint = false;
					}
				}

				// Allocate the array memory, by control point or by polygon vertex.
				int lPolygonVertexCount = lMesh->GetControlPointsCount();
				if (!lAllByControlPoint)
				{
					lPolygonVertexCount = lPolygonCount * 3;
				}
				printf("All By Control Point: %s\n", lAllByControlPoint ? "Yes" : "No");
				vector<float> lVertices;
				lVertices.resize(lPolygonVertexCount * 3);
				vector<unsigned int> lIndices;
				lIndices.resize(lPolygonCount * 3);
				vector<float> lNormals;
				if (lHasNormal)
				{
					lNormals.resize(lPolygonVertexCount * 3);
				}
				vector<float> lUVs;
				FbxStringList lUVNames;
				lMesh->GetUVSetNames(lUVNames);
				const char * lUVName = NULL;
				if (lHasUV && lUVNames.GetCount())
				{
					lUVs.resize(lPolygonVertexCount * 2);
					lUVName = lUVNames[0];
				}

				// Populate the array with vertex attribute, if by control point.
				FbxVector4 * lControlPoints = lMesh->GetControlPoints();
				/////////////////////////
				if ((FbxSkin *)lMesh->GetDeformer(0, FbxDeformer::eSkin) != 0)
				{
					lControlPoints = new FbxVector4[lMesh->GetControlPointsCount()];
					memcpy(lControlPoints, lMesh->GetControlPoints(), lMesh->GetControlPointsCount() * sizeof(FbxVector4));

					FbxTimeSpan lTimeLineTimeSpan;
					pScene->GetGlobalSettings().GetTimelineDefaultTimeSpan(lTimeLineTimeSpan);
					FbxTime pTime = lTimeLineTimeSpan.GetStart();

					// select the base layer from the animation stack
					FbxAnimStack * lCurrentAnimationStack = pScene->GetSrcObject<FbxAnimStack>(0);
					// we assume that the first animation layer connected to the animation stack is the base layer
					// (this is the assumption made in the FBXSDK)
					FbxAnimLayer *mCurrentAnimLayer = lCurrentAnimationStack->GetMember<FbxAnimLayer>();

					// ComputeShapeDeformation(lMesh, pTime, mCurrentAnimLayer, lControlPoints);
					FbxAMatrix lGeometryOffset = GetGeometry(pNode);
					FbxAMatrix lGlobalOffPosition = lGlobalPosition * lGeometryOffset;
					ComputeSkinDeformation(lGlobalOffPosition, lMesh, pTime, lControlPoints, NULL);
				}
				/////////////////////////
				vector<int> lMaterialIndices;
				lMaterialIndices.resize(lPolygonVertexCount);
				vector<int> vertexControlIndices;
				FbxVector4 lCurrentVertex;
				FbxVector4 lCurrentNormal;
				FbxVector2 lCurrentUV;
				if (lAllByControlPoint)
				{
					const FbxGeometryElementNormal * lNormalElement = NULL;
					const FbxGeometryElementUV * lUVElement = NULL;
					if (lHasNormal)
					{
						lNormalElement = lMesh->GetElementNormal(0);
					}
					if (lHasUV)
					{
						lUVElement = lMesh->GetElementUV(0);
					}
					for (int lIndex = 0; lIndex < lPolygonVertexCount; ++lIndex)
					{
						// The material for current face.
						int lMaterialIndex = 0;
						if (lMaterialIndice && lMaterialMappingMode == FbxGeometryElement::eByPolygon)
						{
							lMaterialIndex = lMaterialIndice->GetAt(lIndex);
						}
						// Save the vertex position.
						lCurrentVertex = lControlPoints[lIndex];
						lVertices[lIndex * 3] = static_cast<float>(lCurrentVertex[0]);
						lVertices[lIndex * 3 + 1] = static_cast<float>(lCurrentVertex[1]);
						lVertices[lIndex * 3 + 2] = static_cast<float>(lCurrentVertex[2]);
						lMaterialIndices[lIndex] = lMaterialIndex + lMaterialIndexBase;
						vertexControlIndices.push_back(lIndex);

						// Save the normal.
						if (lHasNormal)
						{
							int lNormalIndex = lIndex;
							if (lNormalElement->GetReferenceMode() == FbxLayerElement::eIndexToDirect)
							{
								lNormalIndex = lNormalElement->GetIndexArray().GetAt(lIndex);
							}
							lCurrentNormal = lNormalElement->GetDirectArray().GetAt(lNormalIndex);
							lNormals[lIndex * 3] = static_cast<float>(lCurrentNormal[0]);
							lNormals[lIndex * 3 + 1] = static_cast<float>(lCurrentNormal[1]);
							lNormals[lIndex * 3 + 2] = static_cast<float>(lCurrentNormal[2]);
						}

						// Save the UV.
						if (lHasUV)
						{
							int lUVIndex = lIndex;
							if (lUVElement->GetReferenceMode() == FbxLayerElement::eIndexToDirect)
							{
								lUVIndex = lUVElement->GetIndexArray().GetAt(lIndex);
							}
							lCurrentUV = lUVElement->GetDirectArray().GetAt(lUVIndex);
							lUVs[lIndex * 2] = static_cast<float>(lCurrentUV[0]);
							lUVs[lIndex * 2 + 1] = static_cast<float>(lCurrentUV[1]);
						}
					}

				}

				int lVertexCount = 0;
				for (int lPolygonIndex = 0; lPolygonIndex < lPolygonCount; ++lPolygonIndex)
				{
					// The material for current face.
					int lMaterialIndex = 0;
					if (lMaterialIndice && lMaterialMappingMode == FbxGeometryElement::eByPolygon)
					{
						lMaterialIndex = lMaterialIndice->GetAt(lPolygonIndex);
					}
					if (lMaterialIndex != 0)
					{
						int i = 0;
					}

					for (int lVerticeIndex = 0; lVerticeIndex < 3; ++lVerticeIndex)
					{
						const int lControlPointIndex = lMesh->GetPolygonVertex(lPolygonIndex, lVerticeIndex);

						if (lAllByControlPoint)
						{
							lIndices[lPolygonIndex * 3 + lVerticeIndex] = static_cast<unsigned int>(lControlPointIndex);
						}
						// Populate the array with vertex attribute, if by polygon vertex.
						else
						{
							lIndices[lPolygonIndex * 3 + lVerticeIndex] = static_cast<unsigned int>(lVertexCount);

							lCurrentVertex = lControlPoints[lControlPointIndex];
							lVertices[lVertexCount * 3] = static_cast<float>(lCurrentVertex[0]);
							lVertices[lVertexCount * 3 + 1] = static_cast<float>(lCurrentVertex[1]);
							lVertices[lVertexCount * 3 + 2] = static_cast<float>(lCurrentVertex[2]);
							lMaterialIndices[lVertexCount] = lMaterialIndex + lMaterialIndexBase;
							vertexControlIndices.push_back(lControlPointIndex);

							if (lHasNormal)
							{
								lMesh->GetPolygonVertexNormal(lPolygonIndex, lVerticeIndex, lCurrentNormal);
								lNormals[lVertexCount * 3] = static_cast<float>(lCurrentNormal[0]);
								lNormals[lVertexCount * 3 + 1] = static_cast<float>(lCurrentNormal[1]);
								lNormals[lVertexCount * 3 + 2] = static_cast<float>(lCurrentNormal[2]);
							}

							if (lHasUV)
							{
								bool lUnmappedUV;
								lMesh->GetPolygonVertexUV(lPolygonIndex, lVerticeIndex, lUVName, lCurrentUV, lUnmappedUV);
								lUVs[lVertexCount * 2] = static_cast<float>(lCurrentUV[0]);
								lUVs[lVertexCount * 2 + 1] = static_cast<float>(lCurrentUV[1]);
							}
						}
						++lVertexCount;
					}
				}
				shape_t shape;
				shape.mesh.indices = lIndices;
				shape.mesh.material_ids = lMaterialIndices;
				shape.mesh.positions = lVertices;
				shape.mesh.normals = lNormals;
				shape.mesh.texcoords = lUVs;
				gShapes.push_back(shape);

				if ((FbxSkin *)lMesh->GetDeformer(0, FbxDeformer::eSkin) != 0)
				{
					delete[] lControlPoints;
				}
				/*
				// For all skins and all clusters, accumulate their deformation and weight
				// on each vertices and store them in lClusterDeformation and lClusterWeight.
				vector<int> vertexJointIndices;
				vertexJointIndices.resize(lMesh->GetControlPointsCount() * 4, -1);
				vector<float> vertexJointWeights;
				vertexJointWeights.resize(lMesh->GetControlPointsCount() * 4, -1);
				vector<vector<float> > jointTransformMatrices;
				int num_frames;

				FbxTimeSpan lTimeLineTimeSpan;
				pScene->GetGlobalSettings().GetTimelineDefaultTimeSpan(lTimeLineTimeSpan);
				FbxTime lStartTime = lTimeLineTimeSpan.GetStart();
				FbxTime lEndTime = lTimeLineTimeSpan.GetStop();
				FbxTime lStepTime = (lEndTime - lStartTime) / 100;
				// select the base layer from the animation stack
				FbxAnimStack * lCurrentAnimationStack = pScene->GetSrcObject<FbxAnimStack>(0);
				// we assume that the first animation layer connected to the animation stack is the base layer
				// (this is the assumption made in the FBXSDK)
				FbxAnimLayer *lCurrentAnimLayer = lCurrentAnimationStack->GetMember<FbxAnimLayer>();
				int lSkinCount = lMesh->GetDeformerCount(FbxDeformer::eSkin);
				for (int lSkinIndex = 0; lSkinIndex < lSkinCount && lSkinIndex < 1; ++lSkinIndex)
				{
					FbxSkin * lSkinDeformer = (FbxSkin *)lMesh->GetDeformer(lSkinIndex, FbxDeformer::eSkin);
					int lClusterCount = lSkinDeformer->GetClusterCount();
					for (int lClusterIndex = 0; lClusterIndex < lClusterCount; ++lClusterIndex)
					{
						FbxCluster* lCluster = lSkinDeformer->GetCluster(lClusterIndex);
						if (!lCluster->GetLink())
							continue;

						FbxAMatrix globalPosition = FbxAMatrix(FbxVector4(0, 0, 0, 1), FbxVector4(0, 0, 0, 0), FbxVector4(1, 1, 1, 1));

						num_frames = 0;
						vector<float> OneJointTransformMatrices;
						for (FbxTime lTime = lStartTime; lTime < lEndTime; lTime += lStepTime)
						{
							FbxAMatrix lVertexTransformMatrix;
							ComputeClusterDeformation(globalPosition, lMesh, lCluster, lVertexTransformMatrix, lTime, 0);
							for (int n = 0; n < 16; n++)
							{
								OneJointTransformMatrices.push_back(*((double*)(lVertexTransformMatrix) + n));
							}
							num_frames++;
						}
						jointTransformMatrices.push_back(OneJointTransformMatrices);

						int lVertexIndexCount = lCluster->GetControlPointIndicesCount();
						for (int k = 0; k < lVertexIndexCount; ++k)
						{
							int lIndex = lCluster->GetControlPointIndices()[k];
							// Sometimes, the mesh can have less points than at the time of the skinning
							// because a smooth operator was active when skinning but has been deactivated during export.
							if (lIndex >= lVertexCount)
								continue;
							double lWeight = lCluster->GetControlPointWeights()[k];
							if (lWeight == 0.0)
							{
								continue;
							}
							int m = 0;
							while (m < 4 && vertexJointIndices[lIndex * 4 + m] != -1)
							{
								m++;
							}
							if (m != 4)
							{
								vertexJointIndices[lIndex * 4 + m] = lClusterIndex;
								vertexJointWeights[lIndex * 4 + m] = (float) lWeight;
							}
						} //For each vertex

					} //lClusterCount
				}
				animation_t animation;
				animation.vertexJointIndices = vertexJointIndices;
				animation.vertexJointWeights = vertexJointWeights;
				animation.jointTransformMatrices = jointTransformMatrices;
				animation.num_frames = num_frames;
				animation.vertexControlIndices = vertexControlIndices;
				gAnimations.push_back(animation);
				*/
			}
		}
	}

	const int lChildCount = pNode->GetChildCount();
	for (int lChildIndex = 0; lChildIndex < lChildCount; ++lChildIndex)
	{
		LoadCacheRecursive(pScene, pNode->GetChild(lChildIndex), lGlobalPosition, pTime);
	}
}

// Deform the vertex array with the shapes contained in the mesh.
void ComputeShapeDeformation(FbxMesh* pMesh, FbxTime& pTime, FbxAnimLayer * pAnimLayer, FbxVector4* pVertexArray)
{
	int lVertexCount = pMesh->GetControlPointsCount();

	FbxVector4* lSrcVertexArray = pVertexArray;
	FbxVector4* lDstVertexArray = new FbxVector4[lVertexCount];
	memcpy(lDstVertexArray, pVertexArray, lVertexCount * sizeof(FbxVector4));

	int lBlendShapeDeformerCount = pMesh->GetDeformerCount(FbxDeformer::eBlendShape);
	for (int lBlendShapeIndex = 0; lBlendShapeIndex < lBlendShapeDeformerCount; ++lBlendShapeIndex)
	{
		FbxBlendShape* lBlendShape = (FbxBlendShape*)pMesh->GetDeformer(lBlendShapeIndex, FbxDeformer::eBlendShape);

		int lBlendShapeChannelCount = lBlendShape->GetBlendShapeChannelCount();
		for (int lChannelIndex = 0; lChannelIndex < lBlendShapeChannelCount; ++lChannelIndex)
		{
			FbxBlendShapeChannel* lChannel = lBlendShape->GetBlendShapeChannel(lChannelIndex);
			if (lChannel)
			{
				// Get the percentage of influence on this channel.
				FbxAnimCurve* lFCurve = pMesh->GetShapeChannel(lBlendShapeIndex, lChannelIndex, pAnimLayer);
				if (!lFCurve) continue;
				double lWeight = lFCurve->Evaluate(pTime);

				/*
				If there is only one targetShape on this channel, the influence is easy to calculate:
				influence = (targetShape - baseGeometry) * weight * 0.01
				dstGeometry = baseGeometry + influence

				But if there are more than one targetShapes on this channel, this is an in-between
				blendshape, also called progressive morph. The calculation of influence is different.

				For example, given two in-between targets, the full weight percentage of first target
				is 50, and the full weight percentage of the second target is 100.
				When the weight percentage reach 50, the base geometry is already be fully morphed
				to the first target shape. When the weight go over 50, it begin to morph from the
				first target shape to the second target shape.

				To calculate influence when the weight percentage is 25:
				1. 25 falls in the scope of 0 and 50, the morphing is from base geometry to the first target.
				2. And since 25 is already half way between 0 and 50, so the real weight percentage change to
				the first target is 50.
				influence = (firstTargetShape - baseGeometry) * (25-0)/(50-0) * 100
				dstGeometry = baseGeometry + influence

				To calculate influence when the weight percentage is 75:
				1. 75 falls in the scope of 50 and 100, the morphing is from the first target to the second.
				2. And since 75 is already half way between 50 and 100, so the real weight percentage change
				to the second target is 50.
				influence = (secondTargetShape - firstTargetShape) * (75-50)/(100-50) * 100
				dstGeometry = firstTargetShape + influence
				*/

				// Find the two shape indices for influence calculation according to the weight.
				// Consider index of base geometry as -1.

				int lShapeCount = lChannel->GetTargetShapeCount();
				double* lFullWeights = lChannel->GetTargetShapeFullWeights();

				// Find out which scope the lWeight falls in.
				int lStartIndex = -1;
				int lEndIndex = -1;
				for (int lShapeIndex = 0; lShapeIndex < lShapeCount; ++lShapeIndex)
				{
					if (lWeight > 0 && lWeight <= lFullWeights[0])
					{
						lEndIndex = 0;
						break;
					}
					if (lWeight > lFullWeights[lShapeIndex] && lWeight < lFullWeights[lShapeIndex + 1])
					{
						lStartIndex = lShapeIndex;
						lEndIndex = lShapeIndex + 1;
						break;
					}
				}

				FbxShape* lStartShape = NULL;
				FbxShape* lEndShape = NULL;
				if (lStartIndex > -1)
				{
					lStartShape = lChannel->GetTargetShape(lStartIndex);
				}
				if (lEndIndex > -1)
				{
					lEndShape = lChannel->GetTargetShape(lEndIndex);
				}

				//The weight percentage falls between base geometry and the first target shape.
				if (lStartIndex == -1 && lEndShape)
				{
					double lEndWeight = lFullWeights[0];
					// Calculate the real weight.
					lWeight = (lWeight / lEndWeight) * 100;
					// Initialize the lDstVertexArray with vertex of base geometry.
					memcpy(lDstVertexArray, lSrcVertexArray, lVertexCount * sizeof(FbxVector4));
					for (int j = 0; j < lVertexCount; j++)
					{
						// Add the influence of the shape vertex to the mesh vertex.
						FbxVector4 lInfluence = (lEndShape->GetControlPoints()[j] - lSrcVertexArray[j]) * lWeight * 0.01;
						lDstVertexArray[j] += lInfluence;
					}
				}
				//The weight percentage falls between two target shapes.
				else if (lStartShape && lEndShape)
				{
					double lStartWeight = lFullWeights[lStartIndex];
					double lEndWeight = lFullWeights[lEndIndex];
					// Calculate the real weight.
					lWeight = ((lWeight - lStartWeight) / (lEndWeight - lStartWeight)) * 100;
					// Initialize the lDstVertexArray with vertex of the previous target shape geometry.
					memcpy(lDstVertexArray, lStartShape->GetControlPoints(), lVertexCount * sizeof(FbxVector4));
					for (int j = 0; j < lVertexCount; j++)
					{
						// Add the influence of the shape vertex to the previous shape vertex.
						FbxVector4 lInfluence = (lEndShape->GetControlPoints()[j] - lStartShape->GetControlPoints()[j]) * lWeight * 0.01;
						lDstVertexArray[j] += lInfluence;
					}
				}
			}//If lChannel is valid
		}//For each blend shape channel
	}//For each blend shape deformer

	memcpy(pVertexArray, lDstVertexArray, lVertexCount * sizeof(FbxVector4));

	delete[] lDstVertexArray;
}

//Compute the transform matrix that the cluster will transform the vertex.
void ComputeClusterDeformation(FbxAMatrix& pGlobalPosition,
	FbxMesh* pMesh,
	FbxCluster* pCluster,
	FbxAMatrix& pVertexTransformMatrix,
	FbxTime pTime,
	FbxPose* pPose)
{
	FbxCluster::ELinkMode lClusterMode = pCluster->GetLinkMode();

	FbxAMatrix lReferenceGlobalInitPosition;
	FbxAMatrix lReferenceGlobalCurrentPosition;
	FbxAMatrix lAssociateGlobalInitPosition;
	FbxAMatrix lAssociateGlobalCurrentPosition;
	FbxAMatrix lClusterGlobalInitPosition;
	FbxAMatrix lClusterGlobalCurrentPosition;

	FbxAMatrix lReferenceGeometry;
	FbxAMatrix lAssociateGeometry;
	FbxAMatrix lClusterGeometry;

	FbxAMatrix lClusterRelativeInitPosition;
	FbxAMatrix lClusterRelativeCurrentPositionInverse;

	if (lClusterMode == FbxCluster::eAdditive && pCluster->GetAssociateModel())
	{
		pCluster->GetTransformAssociateModelMatrix(lAssociateGlobalInitPosition);
		// Geometric transform of the model
		lAssociateGeometry = GetGeometry(pCluster->GetAssociateModel());
		lAssociateGlobalInitPosition *= lAssociateGeometry;
		lAssociateGlobalCurrentPosition = GetGlobalPosition(pCluster->GetAssociateModel(), pTime, pPose);

		pCluster->GetTransformMatrix(lReferenceGlobalInitPosition);
		// Multiply lReferenceGlobalInitPosition by Geometric Transformation
		lReferenceGeometry = GetGeometry(pMesh->GetNode());
		lReferenceGlobalInitPosition *= lReferenceGeometry;
		lReferenceGlobalCurrentPosition = pGlobalPosition;

		// Get the link initial global position and the link current global position.
		pCluster->GetTransformLinkMatrix(lClusterGlobalInitPosition);
		// Multiply lClusterGlobalInitPosition by Geometric Transformation
		lClusterGeometry = GetGeometry(pCluster->GetLink());
		lClusterGlobalInitPosition *= lClusterGeometry;
		lClusterGlobalCurrentPosition = GetGlobalPosition(pCluster->GetLink(), pTime, pPose);

		// Compute the shift of the link relative to the reference.
		//ModelM-1 * AssoM * AssoGX-1 * LinkGX * LinkM-1*ModelM
		pVertexTransformMatrix = lReferenceGlobalInitPosition.Inverse() * lAssociateGlobalInitPosition * lAssociateGlobalCurrentPosition.Inverse() *
			lClusterGlobalCurrentPosition * lClusterGlobalInitPosition.Inverse() * lReferenceGlobalInitPosition;
	}
	else
	{
		pCluster->GetTransformMatrix(lReferenceGlobalInitPosition);
		lReferenceGlobalCurrentPosition = pGlobalPosition;
		// Multiply lReferenceGlobalInitPosition by Geometric Transformation
		lReferenceGeometry = GetGeometry(pMesh->GetNode());
		lReferenceGlobalInitPosition *= lReferenceGeometry;

		// Get the link initial global position and the link current global position.
		pCluster->GetTransformLinkMatrix(lClusterGlobalInitPosition);
		lClusterGlobalCurrentPosition = GetGlobalPosition(pCluster->GetLink(), pTime, pPose);

		// Compute the initial position of the link relative to the reference.
		lClusterRelativeInitPosition = lClusterGlobalInitPosition.Inverse() * lReferenceGlobalInitPosition;

		// Compute the current position of the link relative to the reference.
		lClusterRelativeCurrentPositionInverse = lReferenceGlobalCurrentPosition.Inverse() * lClusterGlobalCurrentPosition;

		// Compute the shift of the link relative to the reference.
		pVertexTransformMatrix = lClusterRelativeCurrentPositionInverse * lClusterRelativeInitPosition;
	}
}

// Deform the vertex array in classic linear way.
void ComputeLinearDeformation(FbxAMatrix& pGlobalPosition,
	FbxMesh* pMesh,
	FbxTime& pTime,
	FbxVector4* pVertexArray,
	FbxPose* pPose)
{
	// All the links must have the same link mode.
	FbxCluster::ELinkMode lClusterMode = ((FbxSkin*)pMesh->GetDeformer(0, FbxDeformer::eSkin))->GetCluster(0)->GetLinkMode();

	int lVertexCount = pMesh->GetControlPointsCount();
	FbxAMatrix* lClusterDeformation = new FbxAMatrix[lVertexCount];
	memset(lClusterDeformation, 0, lVertexCount * sizeof(FbxAMatrix));

	double* lClusterWeight = new double[lVertexCount];
	memset(lClusterWeight, 0, lVertexCount * sizeof(double));

	if (lClusterMode == FbxCluster::eAdditive)
	{
		for (int i = 0; i < lVertexCount; ++i)
		{
			lClusterDeformation[i].SetIdentity();
		}
	}

	// For all skins and all clusters, accumulate their deformation and weight
	// on each vertices and store them in lClusterDeformation and lClusterWeight.
	int lSkinCount = pMesh->GetDeformerCount(FbxDeformer::eSkin);
	for (int lSkinIndex = 0; lSkinIndex < lSkinCount; ++lSkinIndex)
	{
		FbxSkin * lSkinDeformer = (FbxSkin *)pMesh->GetDeformer(lSkinIndex, FbxDeformer::eSkin);

		int lClusterCount = lSkinDeformer->GetClusterCount();
		for (int lClusterIndex = 0; lClusterIndex < lClusterCount; ++lClusterIndex)
		{
			FbxCluster* lCluster = lSkinDeformer->GetCluster(lClusterIndex);
			if (!lCluster->GetLink())
				continue;

			FbxAMatrix lVertexTransformMatrix;
			ComputeClusterDeformation(pGlobalPosition, pMesh, lCluster, lVertexTransformMatrix, pTime, pPose);

			int lVertexIndexCount = lCluster->GetControlPointIndicesCount();
			for (int k = 0; k < lVertexIndexCount; ++k)
			{
				int lIndex = lCluster->GetControlPointIndices()[k];

				// Sometimes, the mesh can have less points than at the time of the skinning
				// because a smooth operator was active when skinning but has been deactivated during export.
				if (lIndex >= lVertexCount)
					continue;

				double lWeight = lCluster->GetControlPointWeights()[k];

				if (lWeight == 0.0)
				{
					continue;
				}

				// Compute the influence of the link on the vertex.
				FbxAMatrix lInfluence = lVertexTransformMatrix;
				MatrixScale(lInfluence, lWeight);

				if (lClusterMode == FbxCluster::eAdditive)
				{
					// Multiply with the product of the deformations on the vertex.
					MatrixAddToDiagonal(lInfluence, 1.0 - lWeight);
					lClusterDeformation[lIndex] = lInfluence * lClusterDeformation[lIndex];

					// Set the link to 1.0 just to know this vertex is influenced by a link.
					lClusterWeight[lIndex] = 1.0;
				}
				else // lLinkMode == FbxCluster::eNormalize || lLinkMode == FbxCluster::eTotalOne
				{
					// Add to the sum of the deformations on the vertex.
					MatrixAdd(lClusterDeformation[lIndex], lInfluence);

					// Add to the sum of weights to either normalize or complete the vertex.
					lClusterWeight[lIndex] += lWeight;
				}
			}//For each vertex			
		}//lClusterCount
	}

	//Actually deform each vertices here by information stored in lClusterDeformation and lClusterWeight
	for (int i = 0; i < lVertexCount; i++)
	{
		FbxVector4 lSrcVertex = pVertexArray[i];
		FbxVector4& lDstVertex = pVertexArray[i];
		double lWeight = lClusterWeight[i];

		// Deform the vertex if there was at least a link with an influence on the vertex,
		if (lWeight != 0.0)
		{
			lDstVertex = lClusterDeformation[i].MultT(lSrcVertex);
			if (lClusterMode == FbxCluster::eNormalize)
			{
				// In the normalized link mode, a vertex is always totally influenced by the links. 
				lDstVertex /= lWeight;
			}
			else if (lClusterMode == FbxCluster::eTotalOne)
			{
				// In the total 1 link mode, a vertex can be partially influenced by the links. 
				lSrcVertex *= (1.0 - lWeight);
				lDstVertex += lSrcVertex;
			}
		}
	}

	delete[] lClusterDeformation;
	delete[] lClusterWeight;
}

// Deform the vertex array in Dual Quaternion Skinning way.
void ComputeDualQuaternionDeformation(FbxAMatrix& pGlobalPosition,
	FbxMesh* pMesh,
	FbxTime& pTime,
	FbxVector4* pVertexArray,
	FbxPose* pPose)
{
	// All the links must have the same link mode.
	FbxCluster::ELinkMode lClusterMode = ((FbxSkin*)pMesh->GetDeformer(0, FbxDeformer::eSkin))->GetCluster(0)->GetLinkMode();

	int lVertexCount = pMesh->GetControlPointsCount();
	int lSkinCount = pMesh->GetDeformerCount(FbxDeformer::eSkin);

	FbxDualQuaternion* lDQClusterDeformation = new FbxDualQuaternion[lVertexCount];
	memset(lDQClusterDeformation, 0, lVertexCount * sizeof(FbxDualQuaternion));

	double* lClusterWeight = new double[lVertexCount];
	memset(lClusterWeight, 0, lVertexCount * sizeof(double));

	// For all skins and all clusters, accumulate their deformation and weight
	// on each vertices and store them in lClusterDeformation and lClusterWeight.
	for (int lSkinIndex = 0; lSkinIndex < lSkinCount; ++lSkinIndex)
	{
		FbxSkin * lSkinDeformer = (FbxSkin *)pMesh->GetDeformer(lSkinIndex, FbxDeformer::eSkin);
		int lClusterCount = lSkinDeformer->GetClusterCount();
		for (int lClusterIndex = 0; lClusterIndex < lClusterCount; ++lClusterIndex)
		{
			FbxCluster* lCluster = lSkinDeformer->GetCluster(lClusterIndex);
			if (!lCluster->GetLink())
				continue;

			FbxAMatrix lVertexTransformMatrix;
			ComputeClusterDeformation(pGlobalPosition, pMesh, lCluster, lVertexTransformMatrix, pTime, pPose);

			FbxQuaternion lQ = lVertexTransformMatrix.GetQ();
			FbxVector4 lT = lVertexTransformMatrix.GetT();
			FbxDualQuaternion lDualQuaternion(lQ, lT);

			int lVertexIndexCount = lCluster->GetControlPointIndicesCount();
			for (int k = 0; k < lVertexIndexCount; ++k)
			{
				int lIndex = lCluster->GetControlPointIndices()[k];

				// Sometimes, the mesh can have less points than at the time of the skinning
				// because a smooth operator was active when skinning but has been deactivated during export.
				if (lIndex >= lVertexCount)
					continue;

				double lWeight = lCluster->GetControlPointWeights()[k];

				if (lWeight == 0.0)
					continue;

				// Compute the influence of the link on the vertex.
				FbxDualQuaternion lInfluence = lDualQuaternion * lWeight;
				if (lClusterMode == FbxCluster::eAdditive)
				{
					// Simply influenced by the dual quaternion.
					lDQClusterDeformation[lIndex] = lInfluence;

					// Set the link to 1.0 just to know this vertex is influenced by a link.
					lClusterWeight[lIndex] = 1.0;
				}
				else // lLinkMode == FbxCluster::eNormalize || lLinkMode == FbxCluster::eTotalOne
				{
					if (lClusterIndex == 0)
					{
						lDQClusterDeformation[lIndex] = lInfluence;
					}
					else
					{
						// Add to the sum of the deformations on the vertex.
						// Make sure the deformation is accumulated in the same rotation direction. 
						// Use dot product to judge the sign.
						double lSign = lDQClusterDeformation[lIndex].GetFirstQuaternion().DotProduct(lDualQuaternion.GetFirstQuaternion());
						if (lSign >= 0.0)
						{
							lDQClusterDeformation[lIndex] += lInfluence;
						}
						else
						{
							lDQClusterDeformation[lIndex] -= lInfluence;
						}
					}
					// Add to the sum of weights to either normalize or complete the vertex.
					lClusterWeight[lIndex] += lWeight;
				}
			}//For each vertex
		}//lClusterCount
	}

	//Actually deform each vertices here by information stored in lClusterDeformation and lClusterWeight
	for (int i = 0; i < lVertexCount; i++)
	{
		FbxVector4 lSrcVertex = pVertexArray[i];
		FbxVector4& lDstVertex = pVertexArray[i];
		double lWeightSum = lClusterWeight[i];

		// Deform the vertex if there was at least a link with an influence on the vertex,
		if (lWeightSum != 0.0)
		{
			lDQClusterDeformation[i].Normalize();
			lDstVertex = lDQClusterDeformation[i].Deform(lDstVertex);

			if (lClusterMode == FbxCluster::eNormalize)
			{
				// In the normalized link mode, a vertex is always totally influenced by the links. 
				lDstVertex /= lWeightSum;
			}
			else if (lClusterMode == FbxCluster::eTotalOne)
			{
				// In the total 1 link mode, a vertex can be partially influenced by the links. 
				lSrcVertex *= (1.0 - lWeightSum);
				lDstVertex += lSrcVertex;
			}
		}
	}

	delete[] lDQClusterDeformation;
	delete[] lClusterWeight;
}

// Deform the vertex array according to the links contained in the mesh and the skinning type.
void ComputeSkinDeformation(FbxAMatrix& pGlobalPosition,
	FbxMesh* pMesh,
	FbxTime& pTime,
	FbxVector4* pVertexArray,
	FbxPose* pPose)
{
	FbxSkin * lSkinDeformer = (FbxSkin *)pMesh->GetDeformer(0, FbxDeformer::eSkin);
	FbxSkin::EType lSkinningType = lSkinDeformer->GetSkinningType();

	if (lSkinningType == FbxSkin::eLinear || lSkinningType == FbxSkin::eRigid)
	{
		ComputeLinearDeformation(pGlobalPosition, pMesh, pTime, pVertexArray, pPose);
	}
	else if (lSkinningType == FbxSkin::eDualQuaternion)
	{
		ComputeDualQuaternionDeformation(pGlobalPosition, pMesh, pTime, pVertexArray, pPose);
	}
	else if (lSkinningType == FbxSkin::eBlend)
	{
		int lVertexCount = pMesh->GetControlPointsCount();

		FbxVector4* lVertexArrayLinear = new FbxVector4[lVertexCount];
		memcpy(lVertexArrayLinear, pMesh->GetControlPoints(), lVertexCount * sizeof(FbxVector4));

		FbxVector4* lVertexArrayDQ = new FbxVector4[lVertexCount];
		memcpy(lVertexArrayDQ, pMesh->GetControlPoints(), lVertexCount * sizeof(FbxVector4));

		ComputeLinearDeformation(pGlobalPosition, pMesh, pTime, lVertexArrayLinear, pPose);
		ComputeDualQuaternionDeformation(pGlobalPosition, pMesh, pTime, lVertexArrayDQ, pPose);

		// To blend the skinning according to the blend weights
		// Final vertex = DQSVertex * blend weight + LinearVertex * (1- blend weight)
		// DQSVertex: vertex that is deformed by dual quaternion skinning method;
		// LinearVertex: vertex that is deformed by classic linear skinning method;
		int lBlendWeightsCount = lSkinDeformer->GetControlPointIndicesCount();
		for (int lBWIndex = 0; lBWIndex < lBlendWeightsCount; ++lBWIndex)
		{
			double lBlendWeight = lSkinDeformer->GetControlPointBlendWeights()[lBWIndex];
			pVertexArray[lBWIndex] = lVertexArrayDQ[lBWIndex] * lBlendWeight + lVertexArrayLinear[lBWIndex] * (1 - lBlendWeight);
		}
	}
}

FbxAMatrix GetGlobalPosition(FbxNode* pNode, const FbxTime& pTime, FbxPose* pPose, FbxAMatrix* pParentGlobalPosition)
{
	FbxAMatrix lGlobalPosition;
	bool        lPositionFound = false;

	if (pPose)
	{
		int lNodeIndex = pPose->Find(pNode);

		if (lNodeIndex > -1)
		{
			// The bind pose is always a global matrix.
			// If we have a rest pose, we need to check if it is
			// stored in global or local space.
			if (pPose->IsBindPose() || !pPose->IsLocalMatrix(lNodeIndex))
			{
				lGlobalPosition = GetPoseMatrix(pPose, lNodeIndex);
			}
			else
			{
				// We have a local matrix, we need to convert it to
				// a global space matrix.
				FbxAMatrix lParentGlobalPosition;

				if (pParentGlobalPosition)
				{
					lParentGlobalPosition = *pParentGlobalPosition;
				}
				else
				{
					if (pNode->GetParent())
					{
						lParentGlobalPosition = GetGlobalPosition(pNode->GetParent(), pTime, pPose);
					}
				}

				FbxAMatrix lLocalPosition = GetPoseMatrix(pPose, lNodeIndex);
				lGlobalPosition = lParentGlobalPosition * lLocalPosition;
			}

			lPositionFound = true;
		}
	}

	if (!lPositionFound)
	{
		// There is no pose entry for that node, get the current global position instead.

		// Ideally this would use parent global position and local position to compute the global position.
		// Unfortunately the equation 
		//    lGlobalPosition = pParentGlobalPosition * lLocalPosition
		// does not hold when inheritance type is other than "Parent" (RSrs).
		// To compute the parent rotation and scaling is tricky in the RrSs and Rrs cases.
		lGlobalPosition = pNode->EvaluateGlobalTransform(pTime);
	}

	return lGlobalPosition;
}

// Get the matrix of the given pose
FbxAMatrix GetPoseMatrix(FbxPose* pPose, int pNodeIndex)
{
	FbxAMatrix lPoseMatrix;
	FbxMatrix lMatrix = pPose->GetMatrix(pNodeIndex);

	memcpy((double*)lPoseMatrix, (double*)lMatrix, sizeof(lMatrix.mData));

	return lPoseMatrix;
}

// Get the geometry offset to a node. It is never inherited by the children.
FbxAMatrix GetGeometry(FbxNode* pNode)
{
	const FbxVector4 lT = pNode->GetGeometricTranslation(FbxNode::eSourcePivot);
	const FbxVector4 lR = pNode->GetGeometricRotation(FbxNode::eSourcePivot);
	const FbxVector4 lS = pNode->GetGeometricScaling(FbxNode::eSourcePivot);

	return FbxAMatrix(lT, lR, lS);
}

// Scale all the elements of a matrix.
void MatrixScale(FbxAMatrix& pMatrix, double pValue)
{
	int i, j;

	for (i = 0; i < 4; i++)
	{
		for (j = 0; j < 4; j++)
		{
			pMatrix[i][j] *= pValue;
		}
	}
}


// Add a value to all the elements in the diagonal of the matrix.
void MatrixAddToDiagonal(FbxAMatrix& pMatrix, double pValue)
{
	pMatrix[0][0] += pValue;
	pMatrix[1][1] += pValue;
	pMatrix[2][2] += pValue;
	pMatrix[3][3] += pValue;
}


// Sum two matrices element by element.
void MatrixAdd(FbxAMatrix& pDstMatrix, FbxAMatrix& pSrcMatrix)
{
	int i, j;

	for (i = 0; i < 4; i++)
	{
		for (j = 0; j < 4; j++)
		{
			pDstMatrix[i][j] += pSrcMatrix[i][j];
		}
	}
}

typedef struct
{
	GLuint vao;
	GLuint vbo;
	GLuint vboTex;
	GLuint ebo;
	int materialId;
	int indexCount;
} Shape;

typedef struct
{
	GLuint texId;
} Material;

vector<Shape> characterShapes;
vector<Material> characterMaterials;
fbx_handles characterFbx;

void My_LoadModels()
{
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;

	std::string err;

	bool ret = LoadFbx(characterFbx, shapes, materials, err, "Running.FBX");

	if (ret)
	{
		// For Each Material
		for (int i = 0; i < materials.size(); i++)
		{
			ILuint ilTexName;
			ilGenImages(1, &ilTexName);
			ilBindImage(ilTexName);
			Material mat;
			if (ilLoadImage(materials[i].diffuse_texname.c_str()))
			{
				int width = ilGetInteger(IL_IMAGE_WIDTH);
				int height = ilGetInteger(IL_IMAGE_HEIGHT);
				unsigned char *data = new unsigned char[width * height * 4];
				ilCopyPixels(0, 0, 0, width, height, 1, IL_RGBA, IL_UNSIGNED_BYTE, data);

				glGenTextures(1, &mat.texId);
				glBindTexture(GL_TEXTURE_2D, mat.texId);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
				glGenerateMipmap(GL_TEXTURE_2D);

				delete[] data;
				ilDeleteImages(1, &ilTexName);
			}
			characterMaterials.push_back(mat);
		}

		// For Each Shape (or Mesh, Object)
		for (int i = 0; i < shapes.size(); i++)
		{
			Shape shape;
			glGenVertexArrays(1, &shape.vao);
			glBindVertexArray(shape.vao);

			glGenBuffers(3, &shape.vbo);
			glBindBuffer(GL_ARRAY_BUFFER, shape.vbo);
			glBufferData(GL_ARRAY_BUFFER, shapes[i].mesh.positions.size() * sizeof(float), shapes[i].mesh.positions.data(), GL_STATIC_DRAW);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
			glBindBuffer(GL_ARRAY_BUFFER, shape.vboTex);
			glBufferData(GL_ARRAY_BUFFER, shapes[i].mesh.texcoords.size() * sizeof(float), shapes[i].mesh.texcoords.data(), GL_STATIC_DRAW);
			glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, 0);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, shape.ebo);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, shapes[i].mesh.indices.size() * sizeof(unsigned int), shapes[i].mesh.indices.data(), GL_STATIC_DRAW);
			shape.materialId = shapes[i].mesh.material_ids[0];
			shape.indexCount = shapes[i].mesh.indices.size();
			glEnableVertexAttribArray(0);
			glEnableVertexAttribArray(1);
			characterShapes.push_back(shape);


		}
	}
}

char** loadShaderSource(const char* file)
{
    FILE* fp = fopen(file, "rb");
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char *src = new char[sz + 1];
    fread(src, sizeof(char), sz, fp);
    src[sz] = '\0';
    char **srcp = new char*[1];
    srcp[0] = src;
    return srcp;
}

void freeShaderSource(char** srcp)
{
    delete[] srcp[0];
    delete[] srcp;
}

//-----------------Begin Load Scene Function and Variables---------------------
GLuint scene_program;
GLint um4mv, um4p;
GLint tex_mode;
GLuint texture_location;
int texture_mode = 0;

//mat4 mvp;
GLint um4mvp;

mat4 view;					// V of MVP, viewing matrix
mat4 projection;			// P of MVP, projection matrix
mat4 model;					// M of MVP, model matrix
mat4 ModelView;
float viewportAspect;
mat4 scaleOne, M;
mat4 model_matrix;


vec3 cameraPos = vec3(217.337f, 233.84f, 117.691f);
vec3 cameraFront = vec3(-0.577186f, -0.374607f, -0.725622f);
vec3 cameraUp = vec3(0.0f, 1.0f, 0.0f);


//Initialize Variable For Mouse Control
vec3 cameraSpeed = vec3(10.0f, 10.0f, 10.0f);
float yaws = -90.0;
float pitchs = 0.0;
bool firstMouse = true;
float lastX = 300, lastY = 300;

GLuint model_program;
GLuint ssao_program;
GLuint depth_program;

GLuint ssao_vao;
GLuint kernal_ubo;
GLuint plane_vao;
GLuint noise_map;


struct
{
	struct
	{
		GLint mv_matrix;
		GLint proj_matrix;
	} toon;

	struct
	{
		GLint normal_map;
		GLint depth_map;
		GLint noise_map;
		GLint noise_scale;
		GLint proj;
	} ssao;

	struct
	{
		GLint mv_matrix;
		GLint proj_matrix;
	} render;
} uniforms;

struct
{
	GLuint fbo;
	GLuint normal_map;
	GLuint depth_map;
} gbuffer;

struct
{
	int width;
	int height;
} viewport_size;

// <---------------------------------------------------- Loader ----------------------------------------------------

typedef struct Vertex {
	vec3 position;
	vec2 texCoords;
	vec3 normal;
	vec3 tangent;
}Vertex;

typedef struct Texture {
	GLuint id;
	aiTextureType type;
	string path;
}Texture;

typedef struct _TextureData
{
	_TextureData(void) :
		width(0),
		height(0),
		data(0)
	{
	}

	int width;
	int height;
	unsigned char* data;
} TextureData;

typedef struct Mesh {
	vector<Vertex> vertexData;
	vector<GLuint> indices;
	vector<Texture> textures;
	GLuint vao, vbo, ebo;

	Mesh() {
		vao = 0;
		vbo = 0;
		ebo = 0;
	}

	void setData(vector<Vertex>& vertData, vector<Texture> & textures, vector<GLuint>& indices)
	{
		this->vertexData = vertData;
		this->indices = indices;
		this->textures = textures;
		if (!vertData.empty())
		{
			if (!indices.empty()) {
				unsigned int vertex_size = sizeof(Vertex);
				unsigned int gluint_size = sizeof(GLuint);
				unsigned int real_f = sizeof(GL_FLOAT);
				GLvoid* first = (GLvoid*)(0);
				GLvoid* second = (GLvoid*)(3 * real_f);
				GLvoid* third = (GLvoid*)(5 * real_f);
				GLvoid* fourth = (GLvoid*)(8 * real_f);
				size_t v_size =  vertex_size * vertexData.size();
				size_t i_size = gluint_size * indices.size();

				glGenVertexArrays(1, &vao);
				glGenBuffers(1, &vbo);
				glGenBuffers(1, &ebo);

				glBindVertexArray(vao);
				glBindBuffer(GL_ARRAY_BUFFER, vbo);
				glBufferData(GL_ARRAY_BUFFER, v_size, &vertexData[0], GL_STATIC_DRAW);

				glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, vertex_size, first);
				glEnableVertexAttribArray(0);

				glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, vertex_size, second);
				glEnableVertexAttribArray(1);

				glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, vertex_size, third);
				glEnableVertexAttribArray(2);

				glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, vertex_size, fourth);
				glEnableVertexAttribArray(3);


				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
				glBufferData(GL_ELEMENT_ARRAY_BUFFER, i_size, &indices[0], GL_STATIC_DRAW);

				glBindBuffer(GL_ARRAY_BUFFER, 0);
				glBindVertexArray(0);
			}
		}
	}
	void Draw(GLuint program) const
	{
		if(vao == 0) {
			return;
			if(vbo == 0) {
				return;
				if (ebo == 0) {
					return;
				}
			}
		}

		glUseProgram(program);
		glBindVertexArray(vao);

		int diffuseCnt = 0, specularCnt = 0, textUnitCnt = 0;

		vector<Texture>::const_iterator it = textures.begin();

		while(it != textures.end()) {
			stringstream samplerNameStr;
			if (it->type == aiTextureType_DIFFUSE) {
				samplerNameStr << "texture_diffuse0";
				GLuint diff_loc = glGetUniformLocation(program, samplerNameStr.str().c_str());
				GLuint id = it->id;
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, id);
				glUniform1i(diff_loc, 0);
			}
			else if (it->type == aiTextureType_HEIGHT) {
				samplerNameStr << "texture_normal0";
				GLuint height_loc = glGetUniformLocation(program, samplerNameStr.str().c_str());
				GLuint id = it->id;
				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_2D, id);
				glUniform1i(height_loc, 1);
			}
			it++;
		}
		glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
		glUseProgram(0);
	}

} Mesh;

typedef struct TextureHelper {
	static  GLuint load2DTexture(const char* filename)
	{
		printf("filename : %s\n", filename);
		GLuint textureId = 0;
		glGenTextures(1, &textureId);
		glBindTexture(0x0DE1, textureId);

		glTexParameteri(0x0DE1, 0x2802, 0x2901);
		glTexParameteri(0x0DE1, 0x2803, 0x2901);

		glTexParameteri(0x0DE1, 0x2800, 0x2601);
		glTexParameteri(0x0DE1, 0x2801, 0x2701);

		TextureData imageTexData = loadPNG(filename);
		int imageWidth = imageTexData.width;
		int imageHeight = imageTexData.height;
		//printf("Loaded image with width[%d], height[%d]\n", imageTexData.width, imageTexData.height);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, imageWidth, imageHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, imageTexData.data);

		glGenerateMipmap(0x0DE1);

		glBindTexture(0x0DE1, 0);
		return textureId;
	}
#define FOURCC_DXT1 0x31545844 
#define FOURCC_DXT3 0x33545844 
#define FOURCC_DXT5 0x35545844 

#define EMPAT 4
#define SATUDUAEMPAT 124

	static GLuint loadDDS(const char * filename) {


		/* try to open the file */
		std::ifstream file(filename, std::ios::in | std::ios::binary);

		/* verify the type of file */
		char filecode[EMPAT];
		file.read(filecode, EMPAT);

		/* get the surface desc */
		char header[SATUDUAEMPAT];
		file.read(header, SATUDUAEMPAT);

		unsigned int height = *(unsigned int*)&(header[8]);
		unsigned int width = *(unsigned int*)&(header[12]);

		unsigned int bufsize;
		if (*(unsigned int*)&(header[24]) > 1) {
			bufsize = *(unsigned int*)&(header[16]) * 2;
		}
		else {
			bufsize = *(unsigned int*)&(header[16]);
		}

		char * buffer = new char[bufsize];

		file.read(buffer, bufsize);
		file.close();

		unsigned int components;

		if (*(unsigned int*)&(header[80]) == 0x31545844) {
			components = 3;
		}
		else {
			components = 4;
		}

		unsigned int format;

		if (*(unsigned int*)&(header[80]) == 0x31545844) {
			format = 0x83F1;
		}
		else if (*(unsigned int*)&(header[80]) == 0x33545844) {
			format = 0x83F2;
		}
		else if (*(unsigned int*)&(header[80]) == 0x35545844) {
			format = 0x83F3;
		}
		else {
			delete[] buffer;
			return 0;
		}

		// Create one OpenGL texture
		GLuint textureID;
		glGenTextures(1, &textureID);

		// "Bind" the newly created texture : all future texture functions will modify this texture
		glBindTexture(0x0DE1, textureID);
		glPixelStorei(0x0CF5, 1);

		unsigned int blockSize;

		if (format == 0x83F1) {
			blockSize = 8;
		}
		else {
			blockSize = 16;
		}
		
		unsigned int offset = 0;
		unsigned int level = 0;
		
		while (level < *(unsigned int*)&(header[24]) && (width || height))
		{
			glCompressedTexImage2D(0x0DE1, level, format, width, height, 0, ((width + 3) / 4)*((height + 3) / 4)*blockSize, buffer + offset);
			offset += ((width + 3) / 4)*((height + 3) / 4)*blockSize;
			width = width / 2;
			height = height / 2;

			// Deal with Non-Power-Of-Two textures. This code is not included in the webpage to reduce clutter.
			if (width < 1) {
				if (1) {
					width = 1;
				}
			}
			if (height < 1) {
				if (1) {
					height = 1;
				}
			}
			++level;
		}

		delete[] buffer;

		return textureID;
	}

	static TextureData loadPNG(const char* const pngFilepath)
	{
		TextureData texture;
		int components;
		// load the texture with stb image, force RGBA (4 components required)
		stbi_uc *data = stbi_load(pngFilepath, &texture.width, &texture.height, &components, 4);

		int texture_width = texture.width;
		int texture_height = texture.height;
		// is the image successfully loaded?
		if (data != NULL)
		{
			texture.data = new unsigned char[texture_width * texture_height * 4 * sizeof(unsigned char)];
			memcpy(texture.data, data, texture_width * texture_height * 4 * sizeof(unsigned char));

			size_t i = 0;
			size_t j = 0;
			size_t k = 0;
			// mirror the image vertically to comply with OpenGL convention
			while (i < texture.width)
			{
				j = 0;
				while (j < texture_height / 2)
				{
					k = 0;
					while (k < 4)
					{
						std::swap(texture.data[(j * texture_width + i) * 4 + k], texture.data[((texture_height - j - 1) * texture_width + i) * 4 + k]);
						++k;
					}
					++j;
				}
				++i;
			}

			// release the loaded image
			stbi_image_free(data);
		}

		return texture;
	}
} TextureHelper;

typedef struct Model {
	std::vector<Mesh> meshes;
	std::string modelFileDir;
	typedef std::map<std::string, Texture> LoadedTextMapType;
	LoadedTextMapType loadedTextureMap;

	bool processNode(const aiNode* node, const aiScene* sceneObjPtr)
	{
		if (!node)
		{
			if (!sceneObjPtr) {
				return false;
			}
			return false;
		}

		size_t i = 0;
		while (i < node->mNumMeshes)
		{
			unsigned int mMeshes_i = node->mMeshes[i];
			const aiMesh* meshPtr = sceneObjPtr->mMeshes[mMeshes_i];
			if (meshPtr != NULL)
			{
				Mesh meshObj;
				if (this->processMesh(meshPtr, sceneObjPtr, meshObj) == true)
				{
					this->meshes.push_back(meshObj);
				}
			}
			++i;
		}

		i = 0;
		while (i < node->mNumChildren)
		{
			aiNode* mChildren_i = node->mChildren[i];
			this->processNode(mChildren_i, sceneObjPtr);
			++i;
		}
		return true;
	}

	bool processMesh(const aiMesh* meshPtr, const aiScene* sceneObjPtr, Mesh& meshObj)
	{
		if (!meshPtr)
		{
			if (!sceneObjPtr) {
				return false;
			}
			return false;
		}
		std::vector<Vertex> vertData;
		std::vector<Texture> textures;
		std::vector<GLuint> indices;

		unsigned int mNumVertices = meshPtr->mNumVertices;

		size_t i = 0;
		while (i < mNumVertices)
		{
			Vertex vertex;
			bool HasPositions = meshPtr->HasPositions();
			bool HasTextureCoords = meshPtr->HasTextureCoords(0);
			bool HasNormals = meshPtr->HasNormals();
			bool HasTangentsAndBitangents = meshPtr->HasTangentsAndBitangents();
			if (HasPositions)
			{
				//glm::mat4 r = rotate(mat4(), radians(rotateAngle), vec3(1.0, 0.0, 0.0));
				aiVector3D mVertices = meshPtr->mVertices[i];
				vertex.position.x = mVertices.x;
				vertex.position.y = mVertices.y;
				vertex.position.z = mVertices.z;

				//vertex.position = vec3(r * vec4(vertex.position, 1.0));
			}

			if (HasTextureCoords)
			{
				aiVector3D mTextureCoords = meshPtr->mTextureCoords[0][i];
				vertex.texCoords.x = mTextureCoords.x;
				vertex.texCoords.y = mTextureCoords.y;
			}
			else
			{
				vertex.texCoords = vec2(0.0f, 0.0f);
			}

			if (HasNormals)
			{
				aiVector3D mNormals = meshPtr->mNormals[i];
				vertex.normal.x = mNormals.x;
				vertex.normal.y = mNormals.y;
				vertex.normal.z = mNormals.z;
			}
			if (HasTangentsAndBitangents)
			{
				aiVector3D mTangents = meshPtr->mTangents[i];
				vertex.tangent.x = mTangents.x;
				vertex.tangent.y = mTangents.y;
				vertex.tangent.z = mTangents.z;
			}
			vertData.push_back(vertex);
			++i;
		}

		i = 0;
		while ( i < meshPtr->mNumFaces)
		{
			unsigned int mNumIndices = meshPtr->mFaces[i].mNumIndices;
			if (mNumIndices != 3)
			{
				std::cerr << "Error:Model::processMesh, mesh not transformed to triangle mesh." << std::endl;
				return false;
			}
			size_t j = 0;
			while (j < mNumIndices)
			{
				indices.push_back(meshPtr->mFaces[i].mIndices[j]);
				++j;
			}
			++i;
		}

		unsigned int mMaterialIndex = meshPtr->mMaterialIndex;
		if (mMaterialIndex >= 0)
		{
			std::vector<Texture> diffuseTexture;
			this->processMaterial(sceneObjPtr->mMaterials[mMaterialIndex], sceneObjPtr, aiTextureType_DIFFUSE, diffuseTexture);
			textures.insert(textures.end(), diffuseTexture.begin(), diffuseTexture.end());

			std::vector<Texture> normalTexture;
			this->processMaterial(sceneObjPtr->mMaterials[mMaterialIndex], sceneObjPtr, aiTextureType_HEIGHT, normalTexture);
			textures.insert(textures.end(), normalTexture.begin(), normalTexture.end());
		}
		meshObj.setData(vertData, textures, indices);
		return true;
	}
	/*
	* Get mesh of texture
	*/
	bool processMaterial(const aiMaterial* matPtr, const aiScene* sceneObjPtr, const aiTextureType textureType, std::vector<Texture>& textures)
	{
		textures.clear();

		if (!matPtr)
		{
			if (!sceneObjPtr) {
				return false;
			}
			return false;
		}

		unsigned int GetTextureCount = matPtr->GetTextureCount(textureType);
		if (GetTextureCount <= 0)
		{
			return true;
		}

		size_t i = 0;
		while (i < matPtr->GetTextureCount(textureType))
		{
			Texture text;
			aiString textPath;
			if (matPtr->GetTexture(textureType, i, &textPath) != aiReturn_SUCCESS || textPath.length == 0)
			{
				std::cerr << "Warning, load texture type=" << textureType
					<< "index= " << i << " failed with return value= "
					<< matPtr->GetTexture(textureType, i, &textPath) << std::endl;
				continue;
			}
			text.path = this->modelFileDir + "/" + textPath.C_Str();
			text.type = textureType;
			//cout << textPath.C_Str() << "\n";
			LoadedTextMapType::const_iterator it = this->loadedTextureMap.find(this->modelFileDir + "/" + textPath.C_Str());
			if (it == this->loadedTextureMap.end())
			{
				GLuint textId = TextureHelper::load2DTexture((this->modelFileDir + "/" + textPath.C_Str()).c_str());
				text.id = textId;
				textures.push_back(text);
				loadedTextureMap[this->modelFileDir + "/" + textPath.C_Str()] = text;
			}
			else
			{
				textures.push_back(it->second);
			}
			++i;
		}
		return true;
	}

	void Draw(GLuint program) const
	{
		std::vector<Mesh>::const_iterator it = this->meshes.begin();
		while( this->meshes.end() != it)
		{
			it->Draw(program);
			++it;
		}
	}

	bool loadModel(const std::string& filePath)
	{
		Assimp::Importer importer;

		const aiScene* sceneObjPtr = importer.ReadFile(
			filePath.c_str(),
			aiProcess_OptimizeGraph |
			aiProcess_OptimizeMeshes |
			aiProcess_ImproveCacheLocality |
			aiProcess_SplitLargeMeshes |
			aiProcess_Triangulate |
			aiProcess_CalcTangentSpace |
			aiProcess_JoinIdenticalVertices |
			aiProcess_SortByPType
		);
		this->modelFileDir = filePath.substr(0, filePath.find_last_of('/'));
		if (!this->processNode(sceneObjPtr->mRootNode, sceneObjPtr))
		{
			std::cerr << "Error:Model::loadModel, process node failed." << std::endl;
			if (1) {
				return false;
			}
		}
		return true;
	}
	~Model()
	{
		std::vector<Mesh>::const_iterator it = this->meshes.begin();
		while (this->meshes.end() != it)
		{
			glDeleteVertexArrays(1, &it->vao);
			glDeleteBuffers(1, &it->vbo);
			glDeleteBuffers(1, &it->ebo);
			++it;
		}
	}
} Model;

Model objModel;
Model objPokemon;
Model objCharmander;
// ---------------------------------------------------- Loader ---------------------------------------------------->


GLuint lightSpaceMatrixLocation;

GLuint depthMapFBO;
GLuint depthMap;
GLuint depthMapLocation;

void shadow_Init()
{
	depth_program = glCreateProgram();
	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	char** vertexShaderSource = loadShaderSource("depth.vs.glsl");
	char** fragmentShaderSource = loadShaderSource("depth.fs.glsl");
	glShaderSource(vertexShader, 1, vertexShaderSource, NULL);
	glShaderSource(fragmentShader, 1, fragmentShaderSource, NULL);
	freeShaderSource(vertexShaderSource);
	freeShaderSource(fragmentShaderSource);
	glCompileShader(vertexShader);
	glCompileShader(fragmentShader);
	shaderLog(vertexShader);
	shaderLog(fragmentShader);
	glAttachShader(depth_program, vertexShader);
	glAttachShader(depth_program, fragmentShader);
	glLinkProgram(depth_program);

	// Gen FBO
	glGenFramebuffers(1, &depthMapFBO);
	// Gen texture
	glGenTextures(1, &depthMap);
	glBindTexture(0x0DE1, depthMap);
	glTexImage2D(0x0DE1, 0, 0x1902,
		SHADOW_WIDTH, SHADOW_HEIGHT, 0, 0x1902, 0x1406, NULL);
	glTexParameteri(0x0DE1, 0x2801, 0x2601);
	glTexParameteri(0x0DE1, 0x2800, 0x2601);
	glTexParameteri(0x0DE1, 0x2802, 0x2901);
	glTexParameteri(0x0DE1, 0x2803, 0x2901);
	// Attach to FBO
	glBindFramebuffer(0x8D40, depthMapFBO);
	glFramebufferTexture2D(0x8D40, 0x8D00, 0x0DE1, depthMap, 0);
	glDrawBuffer(0);
	glReadBuffer(0);
	glBindFramebuffer(0x8D40, 0);
}



/*-----------------------------------------------Skybox part-----------------------------------------------*/
vector<string> faces = { "cubemaps\\face-r.jpg", "cubemaps\\face-l.jpg", "cubemaps\\face-t.jpg", "cubemaps\\face-d.jpg", "cubemaps\\face-b.jpg", "cubemaps\\face-f.jpg" };

GLuint skybox;
GLuint cubemapTexture;
GLuint skyboxVAO;

void skyboxInitFunction()
{
	skybox = glCreateProgram();

	GLuint skybox_vertexShader = glCreateShader(GL_VERTEX_SHADER);
	GLuint skybox_fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

	char** skybox_vertexShaderSource = loadShaderSource("skybox.vs.glsl");
	char** skybox_fragmentShaderSource = loadShaderSource("skybox.fs.glsl");

	glShaderSource(skybox_vertexShader, 1, skybox_vertexShaderSource, NULL);
	glShaderSource(skybox_fragmentShader, 1, skybox_fragmentShaderSource, NULL);

	freeShaderSource(skybox_vertexShaderSource);
	freeShaderSource(skybox_fragmentShaderSource);

	glCompileShader(skybox_vertexShader);
	glCompileShader(skybox_fragmentShader);

	shaderLog(skybox_vertexShader);
	shaderLog(skybox_fragmentShader);

	glAttachShader(skybox, skybox_vertexShader);
	glAttachShader(skybox, skybox_fragmentShader);
	glLinkProgram(skybox);
	glUseProgram(skybox);

	//load skybox Texture
	glGenTextures(1, &cubemapTexture);
	glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
	for (int i = 0; i < 6; i++)
	{
		texture_data envmap_data = loadImg(faces[i].c_str());
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, envmap_data.width, envmap_data.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, envmap_data.data);
	}
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

	glGenVertexArrays(1, &skyboxVAO);
}

void SkyboxRendering()
{
	static const GLfloat gray[] = { 0.2f, 0.2f, 0.2f, 1.0f };
	static const GLfloat ones[] = { 1.0f };

	mat4 mv_matrix = view;
	//cout << mv_matrix[0][0] << endl << mv_matrix[1][0] << endl;

	mat4 inv_vp_matrix = inverse(projection * view);
	//cout << inv_vp_matrix[0][0] << endl << inv_vp_matrix[1][0] << endl;

	glClearBufferfv(GL_COLOR, 0, gray);
	glClearBufferfv(GL_DEPTH, 0, ones);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
	glUniform1i(glGetUniformLocation(skybox, "tex_cubemap"), 0);

	glUseProgram(skybox);
	glBindVertexArray(skyboxVAO);
	glUniformMatrix4fv(glGetUniformLocation(skybox, "inv_vp_matrix"), 1, GL_FALSE, &inv_vp_matrix[0][0]);
	glUniform3fv(glGetUniformLocation(skybox, "eye"), 1, &cameraPos[0]);

	glDisable(GL_DEPTH_TEST);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glEnable(GL_DEPTH_TEST);
}
/*-----------------------------------------------Skybox part-----------------------------------------------*/

/*----------------------------------------------- Terrain part (teacher) -----------------------------------------------*/
GLuint terrain_program;
GLuint tex_displacement;
GLuint tex_color;
GLuint tex_normal;
GLuint terrain_vao;

float dmap_depth;
bool enable_displacement;
bool wireframe;
bool enable_fog;

void Terrain_init()
{
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	terrain_program = glCreateProgram();

	GLuint terrain_vs = glCreateShader(GL_VERTEX_SHADER);
	GLuint terrain_tcs = glCreateShader(GL_TESS_CONTROL_SHADER);
	GLuint terrain_tes = glCreateShader(GL_TESS_EVALUATION_SHADER);
	GLuint terrain_fs = glCreateShader(GL_FRAGMENT_SHADER);

	char** terrain_vs_source = loadShaderSource("terrain_lp.vs.glsl");
	char** terrain_tcs_source = loadShaderSource("terrain_lp.tcs");
	char** terrain_tes_source = loadShaderSource("terrain_lp.tes");
	char** terrain_fs_source = loadShaderSource("terrain_lp.fs.glsl");

	glShaderSource(terrain_vs, 1, terrain_vs_source, NULL);
	glShaderSource(terrain_tcs, 1, terrain_tcs_source, NULL);
	glShaderSource(terrain_tes, 1, terrain_tes_source, NULL);
	glShaderSource(terrain_fs, 1, terrain_fs_source, NULL);

	freeShaderSource(terrain_vs_source);
	freeShaderSource(terrain_tcs_source);
	freeShaderSource(terrain_tes_source);
	freeShaderSource(terrain_fs_source);

	glCompileShader(terrain_vs);
	glCompileShader(terrain_tcs);
	glCompileShader(terrain_tes);
	glCompileShader(terrain_fs);

	shaderLog(terrain_vs);
	shaderLog(terrain_tcs);
	shaderLog(terrain_tes);
	shaderLog(terrain_fs);

	glAttachShader(terrain_program, terrain_vs);
	glAttachShader(terrain_program, terrain_tcs);
	glAttachShader(terrain_program, terrain_tes);
	glAttachShader(terrain_program, terrain_fs);

	glLinkProgram(terrain_program);
	glUseProgram(terrain_program);

	glGenVertexArrays(1, &terrain_vao);
	glBindVertexArray(terrain_vao);

	dmap_depth = 6.0f;

	texture_data tdata = loadImg("terragen.png");
	tdata.data == NULL ? printf("load terrain height image fail\n") : printf("load terrain color height sucessful\n");
	glEnable(GL_TEXTURE_2D);
	glGenTextures(1, &tex_displacement);
	glBindTexture(GL_TEXTURE_2D, tex_displacement);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, tdata.width, tdata.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, tdata.data);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glActiveTexture(GL_TEXTURE1);
	texture_data tdata2 = loadImg("terragen_newColor.png");
	tdata2.data == NULL ? printf("load terrain color image fail\n") : printf("load terrain color image sucessful\n");
	glGenTextures(1, &tex_color);
	glBindTexture(GL_TEXTURE_2D, tex_color);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, tdata2.width, tdata2.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, tdata2.data);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glActiveTexture(GL_TEXTURE2);
	texture_data tdata3 = loadImg("terrNormal.png");
	tdata3.data == NULL ? printf("load terrain normal image fail\n") : printf("load terrain normal image sucessful\n");
	glGenTextures(1, &tex_normal);
	glBindTexture(GL_TEXTURE_2D, tex_normal);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, tdata3.width, tdata3.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, tdata3.data);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glPatchParameteri(GL_PATCH_VERTICES, 4);

	//glEnable(GL_CULL_FACE);

	enable_displacement = true;
	wireframe = false;
	enable_fog = false;
}

void Terrain_rendering()
{
	mat4 model_matrix = mat4(1.0f);
	glm::vec3 scale = glm::vec3(30, 10, 30);
	model_matrix = glm::scale(model_matrix, scale);

	glUseProgram(terrain_program);
	glBindVertexArray(terrain_vao);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, tex_displacement);
	glUniform1i(glGetUniformLocation(terrain_program, "tex_displacement"), 0);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, tex_color);
	glUniform1i(glGetUniformLocation(terrain_program, "tex_color"), 1);

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, tex_normal);
	glUniform1i(glGetUniformLocation(terrain_program, "tex_normal"), 2);

	glUniformMatrix4fv(glGetUniformLocation(terrain_program, "mv_matrix"), 1, GL_FALSE, value_ptr(view * model_matrix));
	glUniformMatrix4fv(glGetUniformLocation(terrain_program, "proj_matrix"), 1, GL_FALSE, value_ptr(projection));
	glUniformMatrix4fv(glGetUniformLocation(terrain_program, "mvp_matrix"), 1, GL_FALSE, value_ptr(projection * view));
	glUniform1f(glGetUniformLocation(terrain_program, "dmap_depth"), enable_displacement ? dmap_depth : 0.0f);
	glUniform1i(glGetUniformLocation(terrain_program, "enable_fog"), enable_fog ? 1 : 0);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	if (wireframe)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	else
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	glDrawArraysInstanced(GL_PATCHES, 0, 4, 64 * 64);
}
/*----------------------------------------------- Terrain part (teacher) -----------------------------------------------*/




glm::mat4 lightViewing = glm::lookAt(glm::vec3(100.0f, 2000.0f, -1000.0f), glm::vec3(1000.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
glm::mat4 lightProj = glm::ortho(-3500.0f, 3500.0f, -3500.0f, 3500.0f, 800.0f, 4000.0f);
glm::mat4 lightSpace = lightProj * lightViewing;


GLuint lightEffect_switch;
int lightEffect = 1;
GLuint normalMap_switch;
int normalMapEffect = 1;
GLuint shadowMap_switch;
int shadowMapEffect = 1;

void initScene() {
	// Create Shader Program
	scene_program = glCreateProgram();

	// Create customize shader by tell openGL specify shader type 
	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

	// Load shader file
	char** vertexShaderSource = loadShaderSource("vertex.vs.glsl");
	char** fragmentShaderSource = loadShaderSource("fragment.fs.glsl");

	// Assign content of these shader files to those shaders we created before
	glShaderSource(vertexShader, 1, vertexShaderSource, NULL);
	glShaderSource(fragmentShader, 1, fragmentShaderSource, NULL);

	// Free the shader file string(won't be used any more)
	freeShaderSource(vertexShaderSource);
	freeShaderSource(fragmentShaderSource);

	// Compile these shaders
	glCompileShader(vertexShader);
	glCompileShader(fragmentShader);

	// Logging
	shaderLog(vertexShader);
	shaderLog(fragmentShader);

	// Assign the program we created before with these shaders
	glAttachShader(scene_program, vertexShader);
	glAttachShader(scene_program, fragmentShader);
	glLinkProgram(scene_program);

	//texture_location = glGetUniformLocation(scene_program, "tex");
	um4mv = glGetUniformLocation(scene_program, "um4mv");
	um4p = glGetUniformLocation(scene_program, "um4p");
	//tex_mode = glGetUniformLocation(scene_program, "tex_mode");

	lightEffect_switch = glGetUniformLocation(scene_program, "lightEffect_switch");
	normalMap_switch = glGetUniformLocation(scene_program, "normalMap_switch");
	shadowMap_switch = glGetUniformLocation(scene_program, "shadowMap_switch");
	depthMapLocation = glGetUniformLocation(scene_program, "depthMap");

	glUseProgram(scene_program);

	//loadScene();
	objModel.loadModel("./eastern ancient casttle.obj");
	objPokemon.loadModel("./TextureModels/Digda.obj");
	objCharmander.loadModel("./TextureModels/Hitokage.obj");
}

void renderScene() {
	glUseProgram(scene_program);

	view = lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
	scaleOne = mat4(1.0f);
	M = scaleOne;
	ModelView = view * M;

	projection = scaleOne * projection;
	glUniformMatrix4fv(um4p, 1, GL_FALSE, value_ptr(projection));
	glUniformMatrix4fv(um4mv, 1, GL_FALSE, value_ptr(ModelView));
	glUniform1i(lightEffect_switch, lightEffect);
	glUniform1i(normalMap_switch, normalMapEffect);
	glUniform1i(shadowMap_switch, shadowMapEffect);
	
	glUniformMatrix4fv(glGetUniformLocation(scene_program, "lightSpaceMatrix"), 1, GL_FALSE, value_ptr(lightSpace));
	glActiveTexture(GL_TEXTURE9);
	glBindTexture(GL_TEXTURE_2D, depthMap);
	glUniform1i(depthMapLocation, 9);

	objModel.Draw(scene_program);
}

void renderModel() {
	glUseProgram(scene_program);

	//model_matrix = translate(mat4(1.0), vec3());
	model_matrix = scale(model_matrix, vec3(15.0f, 15.0f, 15.0f));
	glUniformMatrix4fv(um4mv, 1, GL_FALSE, value_ptr(view * model_matrix));
	objPokemon.Draw(scene_program);
}


void renderModel_Charmander() {
	glUseProgram(scene_program);

	//model_matrix = translate(mat4(1.0), vec3());
	model_matrix = scale(model_matrix, vec3(15.0f, 15.0f, 15.0f));
	glUniformMatrix4fv(um4mv, 1, GL_FALSE, value_ptr(view * model_matrix));
	objCharmander.Draw(scene_program);
}

//-----------------End Load Scene Function and Variables------------------------




/*-----------------------------------------------FRAMEBUFFER POSTPROCESSING------------------------------------*/
GLuint postprocessing_program;
GLuint FBO;
GLuint depthRBO;
GLuint FBODataTexture;
GLuint mainColorTexture;
GLuint vao2;
GLuint window_vertex_buffer;
GLuint filter_mode_location;
GLuint noiseTexture;
int mode = 0;
bool move_bar = true;
int magnify;

static const GLfloat window_vertex[] =
{
	//vec2 position vec2 texture_coord
	1.0f, -1.0f, 1.0f, 0.0f,
   -1.0f, -1.0f, 0.0f, 0.0f,
   -1.0f,  1.0f, 0.0f, 1.0f,
	1.0f,  1.0f, 1.0f, 1.0f
};

void init_post_framebuffer() {

	postprocessing_program = glCreateProgram();

	// Create customize shader by tell openGL specify shader type 
	GLuint vertexShader2 = glCreateShader(GL_VERTEX_SHADER);
	GLuint fragmentShader2 = glCreateShader(GL_FRAGMENT_SHADER);

	// Load shader file
	char** vertexShaderSource2 = loadShaderSource("post_vertex.vs.glsl");
	char** fragmentShaderSource2 = loadShaderSource("post_fragment.fs.glsl");

	// Assign content of these shader files to those shaders we created before
	glShaderSource(vertexShader2, 1, vertexShaderSource2, NULL);
	glShaderSource(fragmentShader2, 1, fragmentShaderSource2, NULL);

	// Free the shader file string(won't be used any more)
	freeShaderSource(vertexShaderSource2);
	freeShaderSource(fragmentShaderSource2);

	// Compile these shaders
	glCompileShader(vertexShader2);
	glCompileShader(fragmentShader2);

	// Logging
	shaderLog(vertexShader2);
	shaderLog(fragmentShader2);

	// Assign the program we created before with these shaders
	glAttachShader(postprocessing_program, vertexShader2);
	glAttachShader(postprocessing_program, fragmentShader2);
	glLinkProgram(postprocessing_program);

	//FBO Vertex Data Settings
	glGenVertexArrays(1, &vao2);
	glBindVertexArray(vao2);

	glGenBuffers(1, &window_vertex_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, window_vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(window_vertex), window_vertex, 0x88E4);

	glVertexAttribPointer(0, 2, 0x1406, 0, sizeof(0x1406) * 4, 0);
	glVertexAttribPointer(1, 2, 0x1406, 0, sizeof(0x1406) * 4, (const GLvoid*)(sizeof(0x1406) * 2));

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);

	//Create FBO
	glGenFramebuffers(1, &FBO);

	filter_mode_location = glGetUniformLocation(postprocessing_program, "mode");

	// load noise texture
	texture_data noise_tex = loadImg("noise.jpg");
	glGenTextures(1, &noiseTexture);
	glBindTexture(GL_TEXTURE_2D, noiseTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, noise_tex.width, noise_tex.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, noise_tex.data);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

void init_post_rbo() {
	//Create Depth RBO
	glDeleteRenderbuffers(1, &depthRBO);
	glDeleteTextures(1, &FBODataTexture);
	glGenRenderbuffers(1, &depthRBO);
	glBindRenderbuffer(GL_RENDERBUFFER, depthRBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT32, viewport_size.width, viewport_size.height);

	// Create fboDataTexture
	// Generate a texture for FBO
	glGenTextures(1, &FBODataTexture);
	// Bind it so that we can specify the format of the textrue
	glBindTexture(0x0DE1, FBODataTexture);

	glTexImage2D(0x0DE1, 0, 0x1908, viewport_size.width, viewport_size.height, 0, 0x1908, 0x1401, NULL);
	glTexParameteri(0x0DE1, 0x2802, 0x812F);
	glTexParameteri(0x0DE1, 0x2803, 0x812F);
	glTexParameteri(0x0DE1, 0x2801, 0x2601);
	glTexParameteri(0x0DE1, 0x2800, 0x2601);

	//Bind the framebuffer with first parameter "GL_DRAW_FRAMEBUFFER" 
	glBindFramebuffer(0x8D40, FBO);

	//Set depthrbo to current fbo
	glFramebufferRenderbuffer(0x8D40, 0x8D00, 0x8D41, depthRBO);

	//Set buffertexture to current fbo
	glFramebufferTexture2D(0x8D40, 0x8CE0, 0x0DE1, FBODataTexture, 0);
}

void post_render() {

	// Return to the default framebuffer
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glClearColor(1.0f, 0.0f, 0.0f, 1.0f);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, mainColorTexture);

	glBindVertexArray(vao2);
	glUseProgram(postprocessing_program);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	glUniform1i(filter_mode_location, mode);
	glUniform1f(glGetUniformLocation(postprocessing_program, "width"), viewport_size.width);
	glUniform1f(glGetUniformLocation(postprocessing_program, "height"), viewport_size.height);
	glUniform1f(glGetUniformLocation(postprocessing_program, "magnify"), magnify);

	float t = glutGet(GLUT_ELAPSED_TIME);
	glUniform1f(glGetUniformLocation(postprocessing_program, "time"), t / 1000);
}
/*-----------------------------------------------FRAMEBUFFER POSTPROCESSING--------------------------------------*/



/*-----------------------------------------------SSAO--------------------------------------*/


int fogEffect = 0;
int ssaoEffect = 0;

GLuint mainFBO;

GLuint mainDepthTexture;
GLuint mainNormalTexture;
GLuint viewSpacePosTex;
GLuint ssaoProgram;

GLuint fogEffect_switch;

void ssaoSetup()
{
	ssaoProgram = glCreateProgram();
	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	char** vertexShaderSource = loadShaderSource("ssao.vs.glsl");
	char** fragmentShaderSource = loadShaderSource("ssao.fs.glsl");
	glShaderSource(vertexShader, 1, vertexShaderSource, NULL);
	glShaderSource(fragmentShader, 1, fragmentShaderSource, NULL);
	freeShaderSource(vertexShaderSource);
	freeShaderSource(fragmentShaderSource);
	glCompileShader(vertexShader);
	glCompileShader(fragmentShader);
	shaderLog(vertexShader);
	shaderLog(fragmentShader);
	glAttachShader(ssaoProgram, vertexShader);
	glAttachShader(ssaoProgram, fragmentShader);
	glLinkProgram(ssaoProgram);

	// Gen FBO
	glGenFramebuffers(1, &mainFBO);
}

void ssao_reshape_setup() {
	// Gen texture
	glGenTextures(1, &mainColorTexture);
	glBindTexture(0x0DE1, mainColorTexture);
	glTexImage2D(0x0DE1, 0, 0x1907, viewport_size.width, viewport_size.height, 0, 0x1907, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(0x0DE1, GL_TEXTURE_MIN_FILTER, 0x2600);
	glTexParameteri(0x0DE1, GL_TEXTURE_MAG_FILTER, 0x2600);
	glGenTextures(1, &mainDepthTexture);
	glBindTexture(0x0DE1, mainDepthTexture);
	glTexImage2D(0x0DE1, 0, GL_DEPTH_COMPONENT32F, viewport_size.width, viewport_size.height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glTexParameteri(0x0DE1, GL_TEXTURE_MIN_FILTER, 0x2600);
	glTexParameteri(0x0DE1, GL_TEXTURE_MAG_FILTER, 0x2600);
	glTexParameteri(0x0DE1, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(0x0DE1, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glGenTextures(1, &mainNormalTexture);
	glBindTexture(0x0DE1, mainNormalTexture);
	glTexImage2D(0x0DE1, 0, GL_RGB16F, viewport_size.width, viewport_size.height, 0, 0x1907, GL_FLOAT, NULL);
	glTexParameteri(0x0DE1, GL_TEXTURE_MIN_FILTER, 0x2600);
	glTexParameteri(0x0DE1, GL_TEXTURE_MAG_FILTER, 0x2600);
	glGenTextures(1, &viewSpacePosTex);
	glBindTexture(0x0DE1, viewSpacePosTex);
	glTexImage2D(0x0DE1, 0, GL_RGB16F, viewport_size.width, viewport_size.height, 0, 0x1907, GL_FLOAT, NULL);
	glTexParameteri(0x0DE1, GL_TEXTURE_MIN_FILTER, 0x2600);
	glTexParameteri(0x0DE1, GL_TEXTURE_MAG_FILTER, 0x2600);
	// Attach to FBO
	glBindFramebuffer(0x8D40, mainFBO);
	glFramebufferTexture2D(0x8D40, GL_COLOR_ATTACHMENT0, 0x0DE1, mainColorTexture, 0);
	glFramebufferTexture2D(0x8D40, GL_COLOR_ATTACHMENT1, 0x0DE1, mainNormalTexture, 0);
	glFramebufferTexture2D(0x8D40, GL_COLOR_ATTACHMENT2, 0x0DE1, viewSpacePosTex, 0);
	glFramebufferTexture2D(0x8D40, GL_DEPTH_ATTACHMENT, 0x0DE1, mainDepthTexture, 0);
	GLuint attachments[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 , GL_COLOR_ATTACHMENT2 };
	glDrawBuffers(3, attachments);
	glBindFramebuffer(0x8D40, 0);
	// Noise map
	glGenTextures(1, &noise_map);
	glBindTexture(0x0DE1, noise_map);
	vec3 noiseData[16];
	for (int i = 0; i < 16; ++i)
	{
		noiseData[i] = normalize(vec3(
			rand() / (float)RAND_MAX, // 0.0 ~ 1.0
			rand() / (float)RAND_MAX, // 0.0 ~ 1.0
			0.0f
		));
	}
	glTexImage2D(0x0DE1, 0, GL_RGB8, 4, 4, 0, 0x1907, GL_FLOAT, &noiseData[0][0]);
	glTexParameteri(0x0DE1, GL_TEXTURE_MAG_FILTER, 0x2600);
	glTexParameteri(0x0DE1, GL_TEXTURE_MIN_FILTER, 0x2600);
	glTexParameteri(0x0DE1, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(0x0DE1, GL_TEXTURE_WRAP_T, GL_REPEAT);
	// Kernal UBO
	glGenBuffers(1, &kernal_ubo);
	glBindBuffer(GL_UNIFORM_BUFFER, kernal_ubo);
	const int numKernels = 32;
	vec4 kernals[numKernels];
	srand(time(NULL));
	for (int i = 0; i < numKernels; ++i)
	{
		float scale = i / numKernels;
		scale = 0.1f + 0.9f * scale * scale;
		kernals[i] = vec4(normalize(vec3(
			rand() / (float)RAND_MAX * 2.0f - 1.0f,
			rand() / (float)RAND_MAX * 2.0f - 1.0f,
			rand() / (float)RAND_MAX * 0.85f + 0.15f)) * scale,
			0.0f
		);
	}
	glBufferData(GL_UNIFORM_BUFFER, numKernels * sizeof(vec4), &kernals[0][0], GL_STATIC_DRAW);

	fogEffect_switch = glGetUniformLocation(ssaoProgram, "fogEffect_switch");
}

void ssao_render() {
	//glBindFramebuffer(0x8D40, 0);
	//glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glUseProgram(ssaoProgram);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(0x0DE1, mainColorTexture);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(0x0DE1, mainNormalTexture);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(0x0DE1, mainDepthTexture);
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(0x0DE1, noise_map);
	glActiveTexture(GL_TEXTURE4);
	glBindTexture(0x0DE1, viewSpacePosTex);
	glUniformMatrix4fv(glGetUniformLocation(ssaoProgram, "proj"), 1, GL_FALSE, &projection[0][0]);
	glUniform2f(glGetUniformLocation(ssaoProgram, "noise_scale"), viewport_size.width / 4.0f, viewport_size.height / 4.0f);
	glUniform1i(glGetUniformLocation(ssaoProgram, "color_map"), 0);
	glUniform1i(glGetUniformLocation(ssaoProgram, "normal_map"), 1);
	glUniform1i(glGetUniformLocation(ssaoProgram, "depth_map"), 2);
	glUniform1i(glGetUniformLocation(ssaoProgram, "noise_map"), 3);
	glUniform1i(glGetUniformLocation(ssaoProgram, "viewSpacePosTex"), 4);
	glBindBufferBase(GL_UNIFORM_BUFFER, 0, kernal_ubo);
	glUniform1i(glGetUniformLocation(ssaoProgram, "enabled"), ssaoEffect);
	glUniform1i(fogEffect_switch, fogEffect);
	//glBindVertexArray(ssao_vao);
	glDisable(GL_DEPTH_TEST);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glEnable(GL_DEPTH_TEST);
}

/*-----------------------------------------------SSAO--------------------------------------*/


/*---------------------------------------Running Man--------------------------------------*/
GLuint program;
int running_man = 1;
std::vector<tinyobj::shape_t> new_shapes;

glm::mat4 running_man_model;

GLfloat running_man_x = 0.0f;
GLfloat running_man_y = 1.0f;
GLfloat running_man_z = 0.0f;

void render_running_man() {
	glUseProgram(scene_program);
	GetFbxAnimation(characterFbx, new_shapes, timer_cnt / 600.0f);

	for (unsigned int i = 0; i < characterShapes.size(); ++i)
	{
		glBindVertexArray(characterShapes[i].vao);
		glBindBuffer(GL_ARRAY_BUFFER, characterShapes[i].vbo);
		glBufferSubData(GL_ARRAY_BUFFER, 0, new_shapes[i].mesh.positions.size() * sizeof(float), new_shapes[i].mesh.positions.data());
		glBindTexture(0x0DE1, characterMaterials[characterShapes[i].materialId].texId);
		running_man_model = translate(mat4(1.0), vec3());
		running_man_model = translate(running_man_model, vec3(running_man_x, running_man_y, running_man_z));
		running_man_model = scale(running_man_model, vec3(0.5f, 0.5f, 0.5f));
		glUniformMatrix4fv(um4mv, 1, GL_FALSE, value_ptr(view * running_man_model));
		glDrawElements(GL_TRIANGLES, characterShapes[i].indexCount, GL_UNSIGNED_INT, 0);
	}
}

/*---------------------------------------Running Man--------------------------------------*/


/*---------------------------------------Rain--------------------------------------*/
#define MAX_PARTICLES 2000
#define RAIN	0
#define SNOW	1
#define	HAIL	2


float slowdown = 1.0;
float velocity = 0.0;
float zoom = -40.0;
float pan = 0.0;
float tilt = 0.0;
float hailsize = 0.1;

int loop;
int fall;

//floor colors
float r = 0.0;
float g = 1.0;
float b = 0.0;
float ground_points[21][21][3];
float ground_colors[21][21][4];
float accum = -10.0;


typedef struct {
	// Life
	bool alive;	// is the particle alive?
	float life;	// particle lifespan
	float fade; // decay
	// color
	float red;
	float green;
	float blue;
	// Position/direction
	float xpos;
	float ypos;
	float zpos;
	// Velocity/Direction, only goes down in y dir
	float vel;
	// Gravity
	float gravity;
}particles;

// Paticle System
particles par_sys[MAX_PARTICLES];

// Initialize/Reset Particles - give them their attributes
void particles_settings(int i) {
	par_sys[i].alive = true;
	par_sys[i].life = 1.0;
	par_sys[i].fade = float(rand() % 100) / 1000.0f + 0.003f;

	par_sys[i].xpos = (float)(rand() % 100) -50;
	par_sys[i].ypos = 15.0;
	par_sys[i].zpos = (float)(rand() % 21) - 10;

	par_sys[i].red = 0.5;
	par_sys[i].green = 0.5;
	par_sys[i].blue = 1.0;

	par_sys[i].vel = velocity;
	par_sys[i].gravity = -0.8;//-0.8;

}

void init_particle() {
	int x, z;

	glShadeModel(GL_SMOOTH);
	glClearColor(0.0, 0.0, 0.0, 0.0);
	glClearDepth(1.0);
	glEnable(GL_DEPTH_TEST);

	// Ground Verticies
	  // Ground Colors
	for (z = 0; z < 21; z++) {
		for (x = 0; x < 21; x++) {
			ground_points[x][z][0] = x - 10.0;
			ground_points[x][z][1] = accum;
			ground_points[x][z][2] = z - 10.0;

			ground_colors[z][x][0] = r; // red value
			ground_colors[z][x][1] = g; // green value
			ground_colors[z][x][2] = b; // blue value
			ground_colors[z][x][3] = 0.0; // acummulation factor
		}
	}

	// Initialize particles
	for (loop = 0; loop < MAX_PARTICLES; loop++) {
		particles_settings(loop);
	}
}

// For Rain
void drawRain() {
	float x, y, z;
	for (loop = 0; loop < MAX_PARTICLES; loop = loop + 2) {
		if (par_sys[loop].alive == true) {
			x = par_sys[loop].xpos;
			y = par_sys[loop].ypos;
			z = par_sys[loop].zpos + zoom;

			// Draw particles
			glColor3f(0.5, 0.5, 1.0);
			glBegin(GL_LINES);
			glVertex3f(x, y, z);
			glVertex3f(x, y + 0.5, z);
			glEnd();

			// Update values
			//Move
			// Adjust slowdown for speed!
			par_sys[loop].ypos += par_sys[loop].vel / (slowdown * 1000);
			par_sys[loop].vel += par_sys[loop].gravity;
			// Decay
			par_sys[loop].life -= par_sys[loop].fade;

			if (par_sys[loop].ypos <= -10) {
				par_sys[loop].life = -1.0;
			}
			//Revive
			if (par_sys[loop].life < 0.0) {
				particles_settings(loop);
			}
		}
	}
}

// Draw Particles
void render_particle() {
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	gluPerspective(45.0f, (float)viewport_size.width / (float)viewport_size.height, 0.1, 3000.0f);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glUseProgram(0);
	int i, j;
	float x, y, z;

	//glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glMatrixMode(GL_MODELVIEW);

	glLoadIdentity();

	glRotatef(pan, 0.0, 1.0, 0.0);
	glRotatef(tilt, 1.0, 0.0, 1.0);

	// along z - y const
	for (i = -10; i + 1 < 11; i++) {
		// along x - y const
		for (j = -10; j + 1 < 11; j++) {
			glColor3fv(ground_colors[i + 10][j + 10]);
			glVertex3f(ground_points[j + 10][i + 10][0],
				ground_points[j + 10][i + 10][1],
				ground_points[j + 10][i + 10][2] + zoom);
			glColor3fv(ground_colors[i + 10][j + 1 + 10]);
			glVertex3f(ground_points[j + 1 + 10][i + 10][0],
				ground_points[j + 1 + 10][i + 10][1],
				ground_points[j + 1 + 10][i + 10][2] + zoom);
			glColor3fv(ground_colors[i + 1 + 10][j + 1 + 10]);
			glVertex3f(ground_points[j + 1 + 10][i + 1 + 10][0],
				ground_points[j + 1 + 10][i + 1 + 10][1],
				ground_points[j + 1 + 10][i + 1 + 10][2] + zoom);
			glColor3fv(ground_colors[i + 1 + 10][j + 10]);
			glVertex3f(ground_points[j + 10][i + 1 + 10][0],
				ground_points[j + 10][i + 1 + 10][1],
				ground_points[j + 10][i + 1 + 10][2] + zoom);
		}

	}

	drawRain();
}

/*---------------------------------------Rain--------------------------------------*/


/*---------------------------------------Particle Flame--------------------------------------*/


#define MAX_PARTICLE_COUNT 1000

struct DrawArraysIndirectCommand
{
	uint count;
	uint primCount;
	uint first;
	uint baseInstance;
};
DrawArraysIndirectCommand defalutDrawArraysCommand = { 0, 1, 0, 0 };

struct Particle
{
	vec3 position;
	float _padding;
	vec3 velocity;
	float lifeTime;
};

struct ParticleBuffer
{
	GLuint shaderStorageBuffer;
	GLuint indirectBuffer;
};

ParticleBuffer particleIn;
ParticleBuffer particleOut;
GLuint particleTexture;
GLuint updateProgram;
GLuint addProgram;
GLuint renderProgram;
GLuint particle_vao;
mat4 mv(1.0f);
mat4 p(1.0f);

const char* add_cs_source[] =
{
	"#version 430 core                                                                                               \n"
	"                                                                                                                \n"
	"layout(local_size_x = 1000, local_size_y = 1, local_size_z = 1) in;                                             \n"
	"                                                                                                                \n"
	"struct Particle                                                                                                 \n"
	"{                                                                                                               \n"
	"    vec3 position;                                                                                              \n"
	"    vec3 velocity;                                                                                              \n"
	"    float lifeTime;                                                                                             \n"
	"};                                                                                                              \n"
	"                                                                                                                \n"
	"layout(std140, binding=0) buffer Particles                                                                      \n"
	"{                                                                                                               \n"
	"     Particle particles[1000];                                                                                  \n"
	"};                                                                                                              \n"
	"                                                                                                                \n"
	"layout(binding = 0, offset = 0) uniform atomic_uint count;                                                      \n"
	"layout(location = 0) uniform uint addCount;                                                                     \n"
	"layout(location = 1) uniform vec2 randomSeed;                                                                   \n"
	"                                                                                                                \n"
	"float rand(vec2 n)                                                                                              \n"
	"{                                                                                                               \n"
	"    return fract(sin(dot(n.xy, vec2(12.9898, 78.233))) * 43758.5453);                                           \n"
	"}                                                                                                               \n"
	"                                                                                                                \n"
	"const float PI2 = 6.28318530718;                                                                                \n"
	"                                                                                                                \n"
	"void main(void)                                                                                                 \n"
	"{                                                                                                               \n"
	"    if(gl_GlobalInvocationID.x < addCount)                                                                      \n"
	"    {                                                                                                           \n"
	"        uint idx = atomicCounterIncrement(count);                                                               \n"
	"        float rand1 = rand(randomSeed + vec2(float(gl_GlobalInvocationID.x * 2)));                              \n"
	"        float rand2 = rand(randomSeed + vec2(float(gl_GlobalInvocationID.x * 2 + 1)));                          \n"
	"        particles[idx].position = vec3(0, 0, 0);                                                                \n"
	"        particles[idx].velocity = normalize(vec3(cos(rand1 * PI2), 5.0 + rand2 * 5.0, sin(rand1 * PI2))) * 0.15;\n"
	"        particles[idx].lifeTime = 0;                                                                            \n"
	"    }                                                                                                           \n"
	"}                                                                                                               \n"
};

const char* update_cs_source[] =
{
	"#version 430 core                                                                                               \n"
	"                                                                                                                \n"
	"layout(local_size_x = 1000, local_size_y = 1, local_size_z = 1) in;                                             \n"
	"                                                                                                                \n"
	"struct Particle                                                                                                 \n"
	"{                                                                                                               \n"
	"    vec3 position;                                                                                              \n"
	"    vec3 velocity;                                                                                              \n"
	"    float lifeTime;                                                                                             \n"
	"};                                                                                                              \n"
	"                                                                                                                \n"
	"layout(std140, binding=0) buffer InParticles                                                                    \n"
	"{                                                                                                               \n"
	"     Particle inParticles[1000];                                                                                \n"
	"};                                                                                                              \n"
	"                                                                                                                \n"
	"layout(std140, binding=1) buffer OutParticles                                                                   \n"
	"{                                                                                                               \n"
	"     Particle outParticles[1000];                                                                               \n"
	"};                                                                                                              \n"
	"                                                                                                                \n"
	"layout(binding = 0, offset = 0) uniform atomic_uint inCount;                                                    \n"
	"layout(binding = 1, offset = 0) uniform atomic_uint outCount;                                                   \n"
	"layout(location = 0) uniform float deltaTime;                                                                   \n"
	"                                                                                                                \n"
	"const vec3 windAccel = vec3(0.015, 0, 0);                                                                       \n"
	"                                                                                                                \n"
	"void main(void)                                                                                                 \n"
	"{                                                                                                               \n"
	"    uint idx = gl_GlobalInvocationID.x;                                                                         \n"
	"    if(idx < atomicCounter(inCount))                                                                            \n"
	"    {                                                                                                           \n"
	"        float lifeTime = inParticles[idx].lifeTime + deltaTime;                                                 \n"
	"        if(lifeTime < 10.0)                                                                                     \n"
	"        {                                                                                                       \n"
	"            uint outIdx = atomicCounterIncrement(outCount);                                                     \n"
	"            outParticles[outIdx].position = inParticles[idx].position + inParticles[idx].velocity * deltaTime;  \n"
	"            outParticles[outIdx].velocity = inParticles[idx].velocity + windAccel * deltaTime;                  \n"
	"            outParticles[outIdx].lifeTime = lifeTime;                                                           \n"
	"        }                                                                                                       \n"
	"    }                                                                                                           \n"
	"}                                                                                                               \n"
};

const char* render_vs_source[] =
{
	"#version 430 core                                                      \n"
	"                                                                       \n"
	"struct Particle                                                        \n"
	"{                                                                      \n"
	"    vec3 position;                                                     \n"
	"    vec3 velocity;                                                     \n"
	"    float lifeTime;                                                    \n"
	"};                                                                     \n"
	"                                                                       \n"
	"layout(std140, binding=0) buffer Particles                             \n"
	"{                                                                      \n"
	"     Particle particles[1000];                                         \n"
	"};                                                                     \n"
	"                                                                       \n"
	"layout(location = 0) uniform mat4 mv;                                  \n"
	"                                                                       \n"
	"out vec4 particlePosition;                                             \n"
	"out float particleSize;                                                \n"
	"out float particleAlpha;                                               \n"
	"                                                                       \n"
	"void main(void)                                                        \n"
	"{                                                                      \n"
	"    particlePosition = mv * vec4(particles[gl_VertexID].position, 1.0);\n"
	"    float lifeTime = particles[gl_VertexID].lifeTime;                  \n"
	"    particleSize = 10.0 + lifeTime * 0.02;                             \n"
	"    particleAlpha = pow((10.0 - lifeTime) * 0.1, 7.0) * 0.7;           \n"
	"}                                                                      \n"
};

const char* render_gs_source[] =
{
	"#version 430 core                                                               \n"
	"                                                                                \n"
	"layout(points, invocations = 1) in;                                             \n"
	"layout(triangle_strip, max_vertices = 4) out;                                   \n"
	"                                                                                \n"
	"layout(location = 1) uniform mat4 p;                                            \n"
	"                                                                                \n"
	"in vec4 particlePosition[];                                                     \n"
	"in float particleSize[];                                                        \n"
	"in float particleAlpha[];                                                       \n"
	"                                                                                \n"
	"out float particleAlphaOut;                                                     \n"
	"out vec2 texcoord;                                                              \n"
	"                                                                                \n"
	"void main(void)                                                                 \n"
	"{                                                                               \n"
	"    vec4 verts[4];                                                              \n"
	"    verts[0] = p * (particlePosition[0] + vec4(-1, -1, 0, 0) * particleSize[0]);\n"
	"    verts[1] = p * (particlePosition[0] + vec4(1, -1, 0, 0) * particleSize[0]); \n"
	"    verts[2] = p * (particlePosition[0] + vec4(1, 1, 0, 0) * particleSize[0]);  \n"
	"    verts[3] = p * (particlePosition[0] + vec4(-1, 1, 0, 0) * particleSize[0]); \n"
	"                                                                                \n"
	"    gl_Position = verts[0];                                                     \n"
	"    particleAlphaOut = particleAlpha[0];                                        \n"
	"    texcoord = vec2(0, 0);                                                      \n"
	"    EmitVertex();                                                               \n"
	"                                                                                \n"
	"    gl_Position = verts[1];                                                     \n"
	"    particleAlphaOut = particleAlpha[0];                                        \n"
	"    texcoord = vec2(1, 0);                                                      \n"
	"    EmitVertex();                                                               \n"
	"                                                                                \n"
	"    gl_Position = verts[3];                                                     \n"
	"    particleAlphaOut = particleAlpha[0];                                        \n"
	"    texcoord = vec2(0, 1);                                                      \n"
	"    EmitVertex();                                                               \n"
	"                                                                                \n"
	"    gl_Position = verts[2];                                                     \n"
	"    particleAlphaOut = particleAlpha[0];                                        \n"
	"    texcoord = vec2(1, 1);                                                      \n"
	"    EmitVertex();                                                               \n"
	"                                                                                \n"
	"    EndPrimitive();                                                             \n"
	"}                                                                               \n"
};

const char* render_fs_source[] =
{
	"#version 430 core                                                 \n"
	"                                                                  \n"
	"layout(binding = 0) uniform sampler2D particleTex;                \n"
	"                                                                  \n"
	"layout(location = 0) out vec4 fragColor;                          \n"
	"                                                                  \n"
	"in float particleAlphaOut;                                        \n"
	"in vec2 texcoord;                                                 \n"
	"                                                                  \n"
	"void main(void)                                                   \n"
	"{                                                                 \n"
	"    fragColor = texture(particleTex, texcoord) * particleAlphaOut;\n"
	"}                                                                 \n"
};

void flame_init() {
	// Create shader program for adding particles.
	GLuint cs = glCreateShader(GL_COMPUTE_SHADER);
	glShaderSource(cs, 1, add_cs_source, NULL);
	glCompileShader(cs);
	addProgram = glCreateProgram();
	glAttachShader(addProgram, cs);
	glLinkProgram(addProgram);
	//printGLShaderLog(cs);

	// Create shader program for updating particles. (from input to output)
	GLuint update_cs = glCreateShader(GL_COMPUTE_SHADER);
	glShaderSource(update_cs, 1, update_cs_source, NULL);
	glCompileShader(update_cs);
	updateProgram = glCreateProgram();
	glAttachShader(updateProgram, update_cs);
	glLinkProgram(updateProgram);
	//printGLShaderLog(cs);

	// Create shader program for rendering particles.
	GLuint vs = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vs, 1, render_vs_source, NULL);
	glCompileShader(vs);
	GLuint gs = glCreateShader(GL_GEOMETRY_SHADER);
	glShaderSource(gs, 1, render_gs_source, NULL);
	glCompileShader(gs);
	GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fs, 1, render_fs_source, NULL);
	glCompileShader(fs);
	renderProgram = glCreateProgram();
	glAttachShader(renderProgram, vs);
	glAttachShader(renderProgram, gs);
	glAttachShader(renderProgram, fs);
	glLinkProgram(renderProgram);
	//printGLShaderLog(vs);
	//printGLShaderLog(gs);
	//printGLShaderLog(fs);

	// Create shader storage buffers & indirect buffers. (which are also used as atomic counter buffers)
	glGenBuffers(1, &particleIn.shaderStorageBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleIn.shaderStorageBuffer);
	glBufferStorage(GL_SHADER_STORAGE_BUFFER, sizeof(Particle) * MAX_PARTICLE_COUNT, NULL, GL_DYNAMIC_STORAGE_BIT);

	glGenBuffers(1, &particleIn.indirectBuffer);
	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, particleIn.indirectBuffer);
	glBufferStorage(GL_DRAW_INDIRECT_BUFFER, sizeof(DrawArraysIndirectCommand), &defalutDrawArraysCommand, GL_DYNAMIC_STORAGE_BIT);

	glGenBuffers(1, &particleOut.shaderStorageBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleOut.shaderStorageBuffer);
	glBufferStorage(GL_SHADER_STORAGE_BUFFER, sizeof(Particle) * MAX_PARTICLE_COUNT, NULL, GL_DYNAMIC_STORAGE_BIT);

	glGenBuffers(1, &particleOut.indirectBuffer);
	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, particleOut.indirectBuffer);
	glBufferStorage(GL_DRAW_INDIRECT_BUFFER, sizeof(DrawArraysIndirectCommand), &defalutDrawArraysCommand, GL_DYNAMIC_STORAGE_BIT);

	// Create particle texture.
	texture_data textureData = loadImg("./smoke.jpg");
	glGenTextures(1, &particleTexture);
	glBindTexture(GL_TEXTURE_2D, particleTexture);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGB8, textureData.width, textureData.height);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, textureData.width, textureData.height, GL_RGBA, GL_UNSIGNED_BYTE, textureData.data);
	glGenerateMipmap(GL_TEXTURE_2D);
	delete[] textureData.data;

	// Create VAO. We don't have any input attributes, but this is still required.
	glGenVertexArrays(1, &particle_vao);
	glBindVertexArray(particle_vao);
}

void AddParticle(uint count)
{
	// Add count particles to input buffers.
	glUseProgram(addProgram);
	glUniform1ui(0, count);
	glUniform2f(1, static_cast<float>(rand()), static_cast<float>(rand()));
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particleIn.shaderStorageBuffer);
	glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, particleIn.indirectBuffer);
	glDispatchCompute(1, 1, 1);
}

void render_flame() {
	//glm::mat4 model_matrix = translate(mat4(1.0f), vec3(78.1096f, 134.54f, -418.567f));
	glm::vec3 scale = glm::vec3(200.0f, 200.0f, 200.0f);
	model_matrix = glm::scale(model_matrix, scale);
	// Update particles.
	glUseProgram(updateProgram);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particleIn.shaderStorageBuffer);
	glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, particleIn.indirectBuffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, particleOut.shaderStorageBuffer);
	glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 1, particleOut.indirectBuffer);
	glUniform1f(0, 0.016f); // We use a fixed update step of 0.016 seconds.
	glNamedBufferSubData(particleOut.indirectBuffer, 0, sizeof(DrawArraysIndirectCommand), &defalutDrawArraysCommand);
	glDispatchCompute(1, 1, 1);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

	//glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glDepthMask(GL_FALSE); // Disable depth writing for additive blending. Remember to turn it on later...

	// Draw particles using updated buffers using additive blending.
	glBindVertexArray(particle_vao);
	glUseProgram(renderProgram);
	glUniformMatrix4fv(0, 1, GL_FALSE, value_ptr(view*model_matrix));
	glUniformMatrix4fv(1, 1, GL_FALSE, value_ptr(projection));
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, particleTexture);
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particleOut.shaderStorageBuffer);
	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, particleOut.indirectBuffer);
	glDrawArraysIndirect(GL_POINTS, 0);

	glDisable(GL_BLEND);
	glDepthMask(GL_TRUE);

	// Swap input and output buffer.
	std::swap(particleIn, particleOut);
}


/*---------------------------------------Particle Flame--------------------------------------*/




/*----------------------------------------------- Water -----------------------------------------------*/
struct WaterColumn
{
	float height;
	float flow;
};

GLuint texture_env;
GLuint program_water;
GLuint program_drop;
GLuint program_render;
GLuint water_vao;
mat4 mvp(1.0f);
float timeElapsed = 0.0f;
GLuint waterBufferIn;
GLuint waterBufferOut;

void AddDrop()
{
	// Randomly add a "drop" of water into the grid system.
	glUseProgram(program_drop);
	glUniform2ui(0, rand() % 180, rand() % 180);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, waterBufferIn);
	glDispatchCompute(10, 10, 1);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void water_init()
{
	// Initialize random seed. Required in AddDrop() function.
	srand(time(NULL));
	{
		program_drop = glCreateProgram();
		GLuint cs = glCreateShader(GL_COMPUTE_SHADER);
		char** water_drop_source = loadShaderSource("water_drop_cs.comp");
		glShaderSource(cs, 1, water_drop_source, NULL);
		freeShaderSource(water_drop_source);
		glCompileShader(cs);
		shaderLog(cs);
		glAttachShader(program_drop, cs);
		glLinkProgram(program_drop);
	}
	{
		program_water = glCreateProgram();
		GLuint cs = glCreateShader(GL_COMPUTE_SHADER);
		char** water_source = loadShaderSource("water_cs.comp");
		glShaderSource(cs, 1, water_source, NULL);
		freeShaderSource(water_source);
		glCompileShader(cs);
		shaderLog(cs);
		glAttachShader(program_water, cs);
		glLinkProgram(program_water);
	}
	{
		program_render = glCreateProgram();

		GLuint vs = glCreateShader(GL_VERTEX_SHADER);
		char** water_render_vs_source = loadShaderSource("water_render.vs.glsl");
		glShaderSource(vs, 1, water_render_vs_source, NULL);
		freeShaderSource(water_render_vs_source);
		glCompileShader(vs);
		shaderLog(vs);

		GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
		char** water_render_fs_source = loadShaderSource("water_render.fs.glsl");
		glShaderSource(fs, 1, water_render_fs_source, NULL);
		freeShaderSource(water_render_fs_source);
		glCompileShader(fs);
		shaderLog(fs);

		glAttachShader(program_render, vs);
		glAttachShader(program_render, fs);
		glLinkProgram(program_render);
	}
	{
		glGenTextures(1, &texture_env);
		glBindTexture(GL_TEXTURE_CUBE_MAP, texture_env);
		for (int i = 0; i < 6; i++)
		{
			texture_data envmap_data = loadImg(faces[i].c_str());
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, envmap_data.width, envmap_data.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, envmap_data.data);
		}
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
	}

	// Create two water grid buffers of 180 * 180 water columns.
	glGenBuffers(1, &waterBufferIn);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, waterBufferIn);
	// Create initial data.
	WaterColumn *data = new WaterColumn[32400];
	for (int x = 0; x < 180; ++x)
	{
		for (int y = 0; y < 180; ++y)
		{
			int idx = y * 180 + x;
			data[idx].height = 60.0f;
			data[idx].flow = 0.0f;
		}
	}
	glBufferStorage(GL_SHADER_STORAGE_BUFFER, sizeof(WaterColumn) * 32400, data, GL_DYNAMIC_STORAGE_BIT);
	delete[] data;

	glGenBuffers(1, &waterBufferOut);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, waterBufferOut);
	glBufferStorage(GL_SHADER_STORAGE_BUFFER, sizeof(WaterColumn) * 32400, NULL, GL_DYNAMIC_STORAGE_BIT);

	glGenVertexArrays(1, &water_vao);
	glBindVertexArray(water_vao);

	// Create an index buffer of 2 triangles, 6 vertices.
	uint indices[] = { 0, 2, 3, 0, 3, 1 };
	GLuint ebo;
	glGenBuffers(1, &ebo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glBufferStorage(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint) * 6, indices, GL_DYNAMIC_STORAGE_BIT);

	AddDrop();
}

bool move_y_axis = false;
void water_rendering()
{
	mat4 model_matrix = mat4(1.0f);
	glm::vec3 scale = glm::vec3(30, 0.1, 30);
	model_matrix = glm::scale(model_matrix, scale);
	if (move_y_axis)
	{
		model_matrix = translate(model_matrix, vec3(0.0, -0.1, 0.0));
		move_y_axis = false;
		//printf("Enter\n");
		//model_matrix -= vec4(0.0, 1.0, 0.0, 0.0);
	}

	// Update water grid.
	glUseProgram(program_water);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, waterBufferIn);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, waterBufferOut);
	// Each group updates 18 * 18 of the grid. We need 10 * 10 groups in total.
	glDispatchCompute(10, 10, 1);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

	// Render water surface.
	//glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);

	glBindVertexArray(water_vao);
	glUseProgram(program_render);
	glUniformMatrix4fv(0, 1, GL_FALSE, value_ptr(projection * view * model_matrix));
	glUniform3f(1, 0, 120, 200);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, waterBufferOut);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, texture_env);
	// Draw 179 * 179 triangles based on the 180 * 180 water grid.
	glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0, 179 * 179);

	std::swap(waterBufferIn, waterBufferOut);
}

void time_function_of_water()
{
	timeElapsed += 0.016;
	if (timeElapsed > 5.0f)
	{
		AddDrop();
		timeElapsed = 0;
		move_y_axis = true;
	}
}
/*----------------------------------------------- Water -----------------------------------------------*/



/*----- Sound Effect -----*/

#include "../Externals/Include/irrKlang/irrKlang.h"
#include "../Externals/Include/irrKlang/ik_ISoundEngine.h"

string background_music_1 = "background_music_1.mp3";
string background_music_2 = "background_music_2.mp3";

bool music_switch = false;
irrklang::ISoundEngine *SoundEngine;

void sound_init()
{
	SoundEngine = irrklang::createIrrKlangDevice();
	music_switch ? SoundEngine->play2D(background_music_2.c_str(), true) : SoundEngine->play2D(background_music_1.c_str(), true);
}

/*----- Sound Effect -----*/




void My_Init()
{
	glClearColor(0.0f, 0.6f, 0.0f, 1.0f);
	glEnable(GL_DEPTH_TEST);
	//glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
	glDepthFunc(GL_LEQUAL);


	program = glCreateProgram();
	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	char** vertexShaderSource = loadShaderSource("vertex2.vs.glsl");
	char** fragmentShaderSource = loadShaderSource("fragment2.fs.glsl");
	glShaderSource(vertexShader, 1, vertexShaderSource, NULL);
	glShaderSource(fragmentShader, 1, fragmentShaderSource, NULL);
	freeShaderSource(vertexShaderSource);
	freeShaderSource(fragmentShaderSource);
	glCompileShader(vertexShader);
	glCompileShader(fragmentShader);
	shaderLog(vertexShader);
	shaderLog(fragmentShader);
	glAttachShader(program, vertexShader);
	glAttachShader(program, fragmentShader);
	glLinkProgram(program);
	um4mvp = glGetUniformLocation(program, "um4mvp");
	glUseProgram(program);

	initScene();
	skyboxInitFunction();

	shadow_Init();

	ssaoSetup();

	init_post_framebuffer();

	Terrain_init();

	water_init();

	flame_init();

	sound_init();

	//init_particle();
}

void My_Display()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	view = lookAt(cameraPos, cameraPos + cameraFront, cameraUp);

	//cout <<  "Camera Positon : " << cameraPos.x << ", " << cameraPos.y << ", " << cameraPos.z << endl;
	//cout << "Camera Front : " << cameraFront.x << ", " << cameraFront.y << ", " << cameraFront.z << endl;
	
	//======================= Begin Shadow Depth Pas=================================
	glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
	glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glUseProgram(depth_program);

	glUniformMatrix4fv(lightSpaceMatrixLocation, 1, GL_FALSE, value_ptr(lightSpace));
	glUniformMatrix4fv(glGetUniformLocation(depth_program, "model"), 1, GL_FALSE, value_ptr(mat4(1.0f)));
	glCullFace(GL_FRONT);
	objModel.Draw(depth_program);

	glUseProgram(depth_program);
	model_matrix = translate(mat4(1.0f), vec3(-17.9465f, 105.38f, -300.96f));
	model_matrix = scale(model_matrix, vec3(15.0f, 15.0f, 15.0f));
	glUniformMatrix4fv(glGetUniformLocation(depth_program, "model"), 1, GL_FALSE, value_ptr(model_matrix));
	objPokemon.Draw(depth_program);

	glCullFace(GL_BACK);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	//=======================End Shadow Depth Pas=================================

	glViewport(0, 0, viewport_size.width, viewport_size.height);

	// Bind Post Processing FBO
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, FBO);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	// Which render buffer attachment is written
	glDrawBuffer(GL_COLOR_ATTACHMENT0);

	glBindFramebuffer(GL_FRAMEBUFFER, mainFBO);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	SkyboxRendering();

	if (running_man == 1) render_running_man();

	model_matrix = translate(mat4(1.0f), vec3(-17.9465f, 105.38f, -300.96f));
	renderModel_Charmander();
	
	model_matrix = translate(mat4(1.0f), vec3(124.226f, 36.6612f, 115.176f));
	renderModel();
	
	model_matrix = translate(mat4(1.0f), vec3(-158.195f, 24.3042f, 274.157f));
	renderModel();

	model_matrix = translate(mat4(1.0f), vec3(51.7317f, 23.0702f, 475.35f));
	renderModel();

	renderScene();

	//render_particle();

	ssao_render();

	if (running_man == 0) render_running_man();

	Terrain_rendering();

	water_rendering();

	model_matrix = translate(mat4(1.0f), vec3(28.7069, 156.08, -408.051));
	render_flame();

	model_matrix = translate(mat4(1.0f), vec3(-65.3328f, 159.48f, -410.454f));
	render_flame();

	model_matrix = translate(mat4(1.0f), vec3(63.6831f, 177.116f, -247.728f));
	render_flame();

	model_matrix = translate(mat4(1.0f), vec3(-100.595f, 181.792f, -250.003f));
	render_flame();
	
	post_render();
	
    glutSwapBuffers();
}

void My_Reshape(int width, int height)
{
	viewport_size.width = width;
	viewport_size.height = height;
	glViewport(0, 0, width, height);
	viewportAspect = (float)width / (float)height;
	projection = perspective(radians(45.0f), viewportAspect, 0.1f, 3000.f);

	//mvp = perspective(radians(45.0f), viewportAspect, 0.1f, 3000.0f);
	//mvp = mvp * lookAt(vec3(-2.0f, 1.0f, 0.0f), vec3(1.0f, 1.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));

	//mvp = perspective(radians(45.0f), viewportAspect, 0.1f, 000.0f);

	ssao_reshape_setup();

	init_post_rbo();

}

void My_Timer(int val)
{
	// Emit 1 particle every 0.016 seconds.
	AddParticle(10);

	time_function_of_water();

	timer_cnt += 3;
	glutPostRedisplay();
	glutTimerFunc(timer_speed, My_Timer, val);
}

void My_Mouse(int button, int state, int x, int y)
{
	if (state == GLUT_DOWN)
	{
		printf("Mouse %d is pressed at (%d, %d)\n", button, x, y);
		if (mode == 2) magnify = 1;
		glUniform1f(glGetUniformLocation(postprocessing_program, "mouse_x"), (float)x);
		glUniform1f(glGetUniformLocation(postprocessing_program, "mouse_y"), (float)y);
		if (button == GLUT_LEFT_BUTTON)
		{
			//glUniform1f(glGetUniformLocation(program2, "bar_position"), (float)x / (float)window_width);
		}
	}
	else if (state == GLUT_UP)
	{
		if (mode == 2) magnify = 0;
		printf("Mouse %d is released at (%d, %d)\n", button, x, y);
	}
}

void onMouseMotion(int x, int y) {
	glUniform1f(glGetUniformLocation(postprocessing_program, "bar_position"), (float)x / (float)viewport_size.width);
}

void onMouseHover(int x, int y) {

	if (firstMouse) {
		lastX = x;
		lastY = y;
		firstMouse = false;
	}

	float xoffset = (x - lastX)* 0.5f;
	float yoffset = (lastY - y)* 0.5f;

	lastX = x;
	lastY = y;

	yaws = yaws + xoffset;
	pitchs = pitchs + yoffset;

	// to limit user not to see upside down scene
	if (pitchs > 89.0f) pitchs = 89.0f;
	if (pitchs < -89.0f) pitchs = -89.0f;

	vec3 direction;
	direction.x = cos(radians(yaws)) * cos(radians(pitchs));
	direction.y = sin(radians(pitchs));
	direction.z = sin(radians(yaws)) * cos(radians(pitchs));
	cameraFront = normalize(direction);
}

void My_Keyboard(unsigned char key, int x, int y)
{
	printf("Key %c is pressed at (%d, %d)\n", key, x, y);
	if (key == 'w' || key == 'W') cameraPos += cameraSpeed * cameraFront;
	if (key == 'a' || key == 'A') cameraPos -= normalize(cross(cameraFront, cameraUp)) * cameraSpeed;
	if (key == 's' || key == 'S') cameraPos -= cameraSpeed * cameraFront;
	if (key == 'd' || key == 'D') cameraPos += normalize(cross(cameraFront, cameraUp)) * cameraSpeed;
	if (key == 'z' || key == 'Z') cameraPos.y += 5.0f;
	if (key == 'x' || key == 'X') cameraPos.y -= 5.0f;

	if (key == 'r' || key == 'R') {
		normalMapEffect = !normalMapEffect;
	}

	if (key == 't' || key == 'T') {
		lightEffect = !lightEffect;
	}

	if (key == 'y' || key == 'Y') {
		shadowMapEffect = !shadowMapEffect;
	}

	if (key == 'q' || key == 'Q') {
		running_man = !running_man;
	}

	if (key == 'm' || key == 'M')
	{
		music_switch = !music_switch;
		SoundEngine->stopAllSounds();
		sound_init();
	}
}

void My_SpecialKeys(int key, int x, int y)
{
	switch(key)
	{
	case GLUT_KEY_F1:
		printf("F1 is pressed at (%d, %d)\n", x, y);
		break;
	case GLUT_KEY_PAGE_UP:
		printf("Page up is pressed at (%d, %d)\n", x, y);
		break;
	case GLUT_KEY_LEFT:
		printf("Left arrow is pressed at (%d, %d)\n", x, y);
		running_man_x += 2.0f;
		break;
	case GLUT_KEY_RIGHT:
		printf("Right arrow is pressed at (%d, %d)\n", x, y);
		running_man_x -= 2.0f;
		break;
	case GLUT_KEY_UP:
		printf("Up arrow is pressed at (%d, %d)\n", x, y);
		running_man_z += 2.0f;
		break;
	case GLUT_KEY_DOWN:
		printf("Up arrow is pressed at (%d, %d)\n", x, y);
		running_man_z -= 2.0f;
		break;
	default:
		printf("Other special key is pressed at (%d, %d)\n", x, y);
		break;
	}
}

void My_Menu(int id)
{
	switch (id)
	{
	case MENU_EXIT:
		exit(0);
		break;

	//post process and rendering effects
	case '0':
		mode = 0;
		break;
	case '1':
		mode = 1;
		break;
	case '2':
		mode = 2;
		break;
	case '3':
		mode = 3;
		break;
	case '4':
		mode = 4;
		break;
	case '5':
		mode = 5;
		break;
	case '6':
		ssaoEffect = !ssaoEffect;
		break;
	case '7':
		fogEffect = !fogEffect;
		enable_fog = !enable_fog;
		break;
	case '8':
		lightEffect = !lightEffect;
		break;
	case '9':
		shadowMapEffect = !shadowMapEffect;
		break;
	case '10':
		normalMapEffect = !normalMapEffect;
		break;
	default:
		break;
	}
}

int main(int argc, char *argv[])
{
#ifdef __APPLE__
	// Change working directory to source code path
	chdir(__FILEPATH__("/../Assets/"));
#endif
	// Initialize GLUT and GLEW, then create a window.
	////////////////////
	glutInit(&argc, argv);
#ifdef _MSC_VER
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
#else
	glutInitDisplayMode(GLUT_3_2_CORE_PROFILE | GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
#endif
	glutInitWindowPosition(100, 100);
	glutInitWindowSize(1066, 600);
	glutCreateWindow("Final Project Team 8 - Dream Island"); // You cannot use OpenGL functions before this line; The OpenGL context must be created first by glutCreateWindow()!
#ifdef _MSC_VER
	glewInit();
#endif
	ilInit();
	ilEnable(IL_ORIGIN_SET);
	ilOriginFunc(IL_ORIGIN_LOWER_LEFT);
	dumpInfo();
	My_Init();
	My_LoadModels();

	// Create a menu and bind it to mouse right button.
	int menu_main = glutCreateMenu(My_Menu);
	int menu_particle = glutCreateMenu(My_Menu);;

	glutSetMenu(menu_main);
	glutAddMenuEntry("Image Abstraction", '0');
	glutAddMenuEntry("Water Color", '1');
	glutAddMenuEntry("Magnifier", '2');
	glutAddMenuEntry("Bloom Effect", '3');
	glutAddMenuEntry("Pixelization", '4');
	glutAddMenuEntry("Sine Wave Distortion", '5');
	glutAddMenuEntry("SSAO", '6');
	glutAddMenuEntry("Fog Effect", '7');
	glutAddMenuEntry("Light Effect", '8');
	glutAddMenuEntry("Shadow Map Effect", '9');
	glutAddMenuEntry("Normal Map Effect", '10');
	glutAddMenuEntry("Exit", MENU_EXIT);

	glutSetMenu(menu_main);
	glutAttachMenu(GLUT_RIGHT_BUTTON);

	// Register GLUT callback functions.
	glutDisplayFunc(My_Display);
	glutReshapeFunc(My_Reshape);
	glutMouseFunc(My_Mouse);
	glutMotionFunc(onMouseMotion);
	glutPassiveMotionFunc(onMouseHover);
	glutKeyboardFunc(My_Keyboard);
	glutSpecialFunc(My_SpecialKeys);
	glutTimerFunc(timer_speed, My_Timer, 0);

	// Enter main event loop.
	glutMainLoop();

	return 0;
}
