//*************************************************************************************
//
// File name: ClassWoodDataPreparation.cpp
//
// Synopsis:  This program prepares the data to perform coarse segmentation 
//            by extracting tiles from the images. 
//
// Copyright © 1992-2024 Zebra Technologies Corp. and/or its affiliates
// All Rights Reserved

#include <windows.h>
#include <mil.h>
#include <string>
#include <random>
#include <numeric>

// ===========================================================================
// Example description.
// ===========================================================================
void PrintHeader()
   {
   MosPrintf(MIL_TEXT("[EXAMPLE NAME]\n")
             MIL_TEXT("ClassWoodDataPreparation\n\n")

             MIL_TEXT("[SYNOPSIS]\n")
             MIL_TEXT("This example shows how to prepare the data to perform coarse segmentation.\n")
             MIL_TEXT("First, it randomly extracts tiles from the images and determines their\n")
             MIL_TEXT("labels using the associated label image.\n")
             MIL_TEXT("Second, it uses blob analysis to locate the defects and extracts a tile\n")
             MIL_TEXT("using the center of gravity of the blob.\n\n")

             MIL_TEXT("[MODULES USED]\n")
             MIL_TEXT("Modules used: application, system, display, buffer, blob, graphic, \n")
             MIL_TEXT("              classification.\n\n"));

   MosPrintf(MIL_TEXT("Press <Enter> to continue.\n\n"));
   MosGetch();
   }

// Path definitions.
#define IMAGE_ROOT_PATH M_IMAGE_PATH MIL_TEXT("/Classification/ClassWoodDataPreparation/")
#define EXAMPLE_IMAGE_PATH           IMAGE_ROOT_PATH MIL_TEXT("Data/Images/")
#define EXAMPLE_LABEL_PATH           IMAGE_ROOT_PATH MIL_TEXT("Data/Labels/")
#define EXAMPLE_DEST_DATA_PATH       MIL_TEXT("Dest\\")

// First crop larger tiles to have data during augmentaiton for overscan. 
static const MIL_INT NO_AUG_IMAGE_SIZE = 140;

// Size of the tiles that will be used for training. 
static const MIL_INT TILE_IMAGE_SIZE = 115;

// Define the retina size.
// Label each tile using a smaller retina inside the tile.
// The larger the size of retina, the coarser the result of segmentation.
// The size come as percentage of Width and Height of the tile.
static const MIL_INT LABEL_RETINA_SIZE = 16;

// How many tiles to extract randomly from each image.
static const MIL_INT NB_RAND_TILES_PER_IMAGE = 15;

// Define the classes.
static const MIL_INT NUMBER_OF_CLASSES = 3;
MIL_STRING CLASS_NAMES[NUMBER_OF_CLASSES] = {MIL_TEXT("NoDefect"),
                                             MIL_TEXT("LargeKnots"),
                                             MIL_TEXT("SmallKnots")};

// Icon image for each class.
MIL_STRING CLASS_ICONS[NUMBER_OF_CLASSES] = {IMAGE_ROOT_PATH MIL_TEXT("Data\\NoDefect.mim"),
                                             IMAGE_ROOT_PATH MIL_TEXT("Data\\LargeKnots.mim"),
                                             IMAGE_ROOT_PATH MIL_TEXT("Data\\SmallKnots.mim")};

// Define the associated value of each class in the label image.
MIL_INT CLASS_LABEL_VALUES[NUMBER_OF_CLASSES] = {0,1,2};

// How many times to perform augmentation on the tiles of each class.
// Augmentation can help to balance the dataset. 
MIL_INT NB_AUGMENTATION_PER_IMAGE[NUMBER_OF_CLASSES] = {1, 9, 9};

MIL_STRING GetExampleCurrentDirectory();

const std::vector<MIL_INT> CreateShuffledIndex(MIL_INT NbEntries, unsigned int Seed);

void DeleteFiles(const std::vector<MIL_STRING>& Files);

void ListFilesInFolder(const MIL_ID MilApplication, const MIL_STRING& FolderName, std::vector<MIL_STRING>& FilesInFolder);

void DeleteFilesInFolder(const MIL_ID MilApplication, const MIL_STRING& FolderName);

void AddClassDefinitions(MIL_ID MilSystem,
                         MIL_ID Dataset,
                         const MIL_STRING* ClassNames,
                         const MIL_STRING* ClassIcons,
                         MIL_INT NumberOfClasses);

void ExtractRandomTiles(MIL_ID MilSystem,
                        MIL_ID SourceDataset,
                        MIL_INT NbTiles,
                        MIL_INT SizeX,
                        MIL_INT SizeY,
                        MIL_STRING ImagesPath,
                        MIL_STRING LabelsPath,
                        MIL_STRING DestPath,
                        MIL_STRING* ClassNames,
                        MIL_ID DestDataset);

void ExtractCoGTiles(MIL_ID MilSystem,
                     MIL_ID SourceDataset,
                     MIL_INT NbClasses,
                     MIL_INT SizeX,
                     MIL_INT SizeY,
                     MIL_STRING ImagesPath,
                     MIL_STRING LabelsPath,
                     MIL_STRING DestPath,
                     MIL_STRING* ClassNames,
                     MIL_ID DestDataset);

void PrepareExampleDataFolder(const MIL_ID MilApplication, const MIL_STRING& ExampleDataPath, const MIL_STRING* ClassName, MIL_INT NumberOfClasses);

void AddFolderToDataset(const MIL_ID MilApplication, const MIL_STRING& DataPath, MIL_ID Dataset);

void AugmentDataset(MIL_ID System, MIL_ID Dataset, const MIL_INT* NbAugmentPerImage);

void CropDatasetImages(MIL_ID MilSystem, MIL_ID Dataset, MIL_INT FinalImageSize);

MIL_DOUBLE GetRetinaLabel(MIL_ID MilSystem, MIL_ID LabelImage, MIL_INT RetinaSizeX, MIL_INT RetinaSizeY);

MIL_UNIQUE_BUF_ID CreateImageOfAllClasses(MIL_ID MilSystem,
                                          const MIL_STRING* ClassIcons,
                                          const MIL_STRING* ClassNames,
                                          MIL_INT NumberOfClasses);


// ****************************************************************************
//    Main.
// ****************************************************************************
int MosMain()
   {
   PrintHeader();

   MIL_UNIQUE_APP_ID MilApplication = MappAlloc(M_NULL, M_DEFAULT, M_UNIQUE_ID);
   MIL_UNIQUE_SYS_ID MilSystem = MsysAlloc(M_DEFAULT, M_SYSTEM_HOST, M_DEFAULT, M_DEFAULT, M_UNIQUE_ID);

   // Display sample tiles.
   MIL_UNIQUE_DISP_ID MilDisplay = MdispAlloc(MilSystem, M_DEFAULT, MIL_TEXT("M_DEFAULT"), M_DEFAULT, M_UNIQUE_ID);

   // Display a representative image of all classes.
   MIL_UNIQUE_BUF_ID AllClassesImage = CreateImageOfAllClasses(MilSystem, CLASS_ICONS, CLASS_NAMES, NUMBER_OF_CLASSES);
   MdispSelect(MilDisplay, AllClassesImage);

   MosPrintf(MIL_TEXT("Preparing the tiles... \n"));

   // If the destination does not already exist we will create the appropriate
   // ExampleDataPath folders structure.
   // If the structure is already existing, then we will remove previous
   // data to ensure repeatability.
   PrepareExampleDataFolder(MilApplication, EXAMPLE_DEST_DATA_PATH, CLASS_NAMES, NUMBER_OF_CLASSES);

   // We create a dataset with all the data
   MosPrintf(MIL_TEXT("\nCreating the dataset containing all the fullframe data...\n"));

   // Create the datasets.
   MIL_UNIQUE_CLASS_ID FullFrameDataset    = MclassAlloc(MilSystem, M_DATASET_IMAGES, M_DEFAULT, M_UNIQUE_ID);
   MIL_UNIQUE_CLASS_ID WorkingTrainDataset = MclassAlloc(MilSystem, M_DATASET_IMAGES, M_DEFAULT, M_UNIQUE_ID);
   MIL_UNIQUE_CLASS_ID WorkingDevDataset   = MclassAlloc(MilSystem, M_DATASET_IMAGES, M_DEFAULT, M_UNIQUE_ID);
   MIL_UNIQUE_CLASS_ID TrainDataset        = MclassAlloc(MilSystem, M_DATASET_IMAGES, M_DEFAULT, M_UNIQUE_ID);
   MIL_UNIQUE_CLASS_ID DevDataset          = MclassAlloc(MilSystem, M_DATASET_IMAGES, M_DEFAULT, M_UNIQUE_ID);

   MclassControl(FullFrameDataset   , M_CONTEXT, M_ROOT_PATH, EXAMPLE_IMAGE_PATH);
   MclassControl(WorkingTrainDataset, M_CONTEXT, M_ROOT_PATH, EXAMPLE_IMAGE_PATH);
   MclassControl(WorkingDevDataset  , M_CONTEXT, M_ROOT_PATH, EXAMPLE_IMAGE_PATH);
   MclassControl(TrainDataset       , M_CONTEXT, M_ROOT_PATH, GetExampleCurrentDirectory());
   MclassControl(DevDataset         , M_CONTEXT, M_ROOT_PATH, GetExampleCurrentDirectory());

   AddClassDefinitions(MilSystem, FullFrameDataset, CLASS_NAMES, CLASS_ICONS, NUMBER_OF_CLASSES);
   MclassCopy(FullFrameDataset, M_DEFAULT, TrainDataset, M_DEFAULT, M_CLASS_DEFINITIONS, M_DEFAULT);
   MclassCopy(FullFrameDataset, M_DEFAULT, DevDataset, M_DEFAULT, M_CLASS_DEFINITIONS, M_DEFAULT);

   // Add all the images into a dataset. 
   AddFolderToDataset(MilApplication, EXAMPLE_IMAGE_PATH, FullFrameDataset);

   MosPrintf(MIL_TEXT("\nSplitting the fullframe dataset to train/dev datasets...\n"));

   // We want to split: Train=80% and Dev=20%.
   const MIL_DOUBLE PERCENTAGE_IN_TRAIN_DATASET = 80.0;

   // Split the dataset to train and dev datasets.
   MclassSplitDataset(M_SPLIT_CONTEXT_FIXED_SEED, FullFrameDataset, WorkingTrainDataset, WorkingDevDataset,
                      PERCENTAGE_IN_TRAIN_DATASET, M_NULL, M_DEFAULT);

   // There are different methods of extracting tiles from an image.
   // Tiles could be randomly extracted from the image,
   // or could be extracted using a grid,
   // or using blob analysis.
   // When using blob analysis, the center of gravity of the blob could be used to extract the tiles. 

   MosPrintf(MIL_TEXT("\nExtract random tiles from the trainset...\n"));

   // Randomly extract tiles and add them to the dataset.
   ExtractRandomTiles(MilSystem,
                      WorkingTrainDataset,
                      NB_RAND_TILES_PER_IMAGE,
                      NO_AUG_IMAGE_SIZE,
                      NO_AUG_IMAGE_SIZE,
                      EXAMPLE_IMAGE_PATH,
                      EXAMPLE_LABEL_PATH,
                      EXAMPLE_DEST_DATA_PATH,
                      CLASS_NAMES,
                      TrainDataset);

   MosPrintf(MIL_TEXT("\nExtract random tiles from the devset...\n"));
   // Randomly extract tiles and add them to the dataset.
   ExtractRandomTiles(MilSystem,
                      WorkingDevDataset,
                      NB_RAND_TILES_PER_IMAGE,
                      NO_AUG_IMAGE_SIZE,
                      NO_AUG_IMAGE_SIZE,
                      EXAMPLE_IMAGE_PATH,
                      EXAMPLE_LABEL_PATH,
                      EXAMPLE_DEST_DATA_PATH,
                      CLASS_NAMES,
                      DevDataset);

   MosPrintf(MIL_TEXT("\nExtract CoG tiles from the trainset...\n"));
   // Use CoG to extract tiles and add them to the dataset
   ExtractCoGTiles(MilSystem,
                   WorkingTrainDataset,
                   NUMBER_OF_CLASSES,
                   NO_AUG_IMAGE_SIZE,
                   NO_AUG_IMAGE_SIZE,
                   EXAMPLE_IMAGE_PATH,
                   EXAMPLE_LABEL_PATH,
                   EXAMPLE_DEST_DATA_PATH,
                   CLASS_NAMES,
                   TrainDataset);

   MosPrintf(MIL_TEXT("\nExtract CoG tiles from the devset...\n"));
   // Use CoG to extract tiles and add them to the dataset.
   ExtractCoGTiles(MilSystem,
                   WorkingDevDataset,
                   NUMBER_OF_CLASSES,
                   NO_AUG_IMAGE_SIZE,
                   NO_AUG_IMAGE_SIZE,
                   EXAMPLE_IMAGE_PATH,
                   EXAMPLE_LABEL_PATH,
                   EXAMPLE_DEST_DATA_PATH,
                   CLASS_NAMES,
                   DevDataset);

   MosPrintf(MIL_TEXT("\nAugmenting the train dataset...\n"));

   // Perform data augmentation to the TrainDataset.
   AugmentDataset(MilSystem, TrainDataset, NB_AUGMENTATION_PER_IMAGE);

   // Crop the dataset images to ensure that they have the required size for the application.
   MosPrintf(MIL_TEXT("\nCropping images from the train/dev datasets.\n"));

   MosPrintf(MIL_TEXT("\nCropping images from the train dataset...\n"));
   CropDatasetImages(MilSystem, TrainDataset, TILE_IMAGE_SIZE);

   MosPrintf(MIL_TEXT("\nCropping images from the dev dataset...\n"));
   CropDatasetImages(MilSystem, DevDataset, TILE_IMAGE_SIZE);

   // Save the datasets.
   MclassSave(MIL_TEXT("TrainDataset.mclassd"), TrainDataset, M_DEFAULT);
   MclassSave(MIL_TEXT("DevDataset.mclassd"), DevDataset, M_DEFAULT);

   // Useful to export entries from different sets if one wants to ensure that
   // data preparation has worked as expected. Uncomment if required.
   //MclassExport(MIL_TEXT("TrainDataset.csv"), M_FORMAT_CSV, TrainDataset, M_DEFAULT, M_ENTRIES, M_DEFAULT);
   //MclassExport(MIL_TEXT("DevDataset.csv"), M_FORMAT_CSV, DevDataset, M_DEFAULT, M_ENTRIES, M_DEFAULT);


   return 0;
   }


// This function extracts random tiles from images and adds them to the dataset. 
void ExtractRandomTiles(MIL_ID MilSystem,
                        MIL_ID SourceDataset,
                        MIL_INT NbTiles,
                        MIL_INT TileSizeX,
                        MIL_INT TileSizeY,
                        MIL_STRING ImagesPath,
                        MIL_STRING LabelsPath,
                        MIL_STRING DestPath,
                        MIL_STRING* ClassNames,
                        MIL_ID DestDataset)
   {
   // Inquire the number of images already added to the datasets. 
   MIL_INT SrcNbEntries, DstNbEntries, CurImageIndex = 0;
   MclassInquire(SourceDataset, M_DEFAULT, M_NUMBER_OF_ENTRIES + M_TYPE_MIL_INT, &SrcNbEntries);
   MclassInquire(DestDataset, M_DEFAULT, M_NUMBER_OF_ENTRIES + M_TYPE_MIL_INT, &DstNbEntries);

   for(MIL_INT ind = 0; ind < SrcNbEntries; ind++)
      {
      MosPrintf(MIL_TEXT("   %d of %d completed\r"), ind + 1, SrcNbEntries);

      // Get the filenames.
      MIL_STRING FileName, ImgPath, LblPath;
      MclassInquireEntry(SourceDataset, ind, M_DEFAULT_KEY, M_DEFAULT, M_FILE_PATH, FileName);
      ImgPath = ImagesPath + FileName;
      LblPath = LabelsPath + FileName;

      // Load the original image and the label image. 
      auto OriginalImage = MbufRestore(ImgPath, MilSystem, M_UNIQUE_ID);
      auto OriginalLabel = MbufRestore(LblPath, MilSystem, M_UNIQUE_ID);

      MIL_INT ImageSizeX = MbufInquire(OriginalImage, M_SIZE_X, M_NULL);
      MIL_INT ImageSizeY = MbufInquire(OriginalImage, M_SIZE_Y, M_NULL);
      MIL_INT ImageSizeBand = MbufInquire(OriginalImage, M_SIZE_BAND, M_NULL);

      // Allocate the buffers for the image and label tiles. 
      auto MilTileImg = MbufAllocColor(MilSystem, ImageSizeBand, TileSizeX, TileSizeY, 8 + M_UNSIGNED, M_IMAGE + M_PROC, M_UNIQUE_ID);
      auto MilTileLbl = MbufAlloc2d(MilSystem, TileSizeX, TileSizeY, 8 + M_UNSIGNED, M_IMAGE + M_PROC, M_UNIQUE_ID);

      // The tile should reside inside the orignal image. 
      MIL_INT OffsetX, OffsetY;
      MIL_INT MaxOffsetX = ImageSizeX - TileSizeX - 1;
      MIL_INT MaxOffsetY = ImageSizeY - TileSizeY - 1;

      // For each image generates N tiles. 
      for(int TileIndex = 1; TileIndex < NbTiles; TileIndex++)
         {
         // Generate random position. 
         OffsetX = rand() % MaxOffsetX;
         OffsetY = rand() % MaxOffsetY;

         MbufCopyColor2d(OriginalImage, MilTileImg, M_ALL_BANDS, OffsetX, OffsetY, M_ALL_BANDS, 0, 0, TileSizeX, TileSizeY);
         MbufCopyColor2d(OriginalLabel, MilTileLbl, M_ALL_BANDS, OffsetX, OffsetY, M_ALL_BANDS, 0, 0, TileSizeX, TileSizeY);

         // Compute the ground truth label of the extracted tile. 
         MIL_DOUBLE GroundTruth = GetRetinaLabel(MilSystem, MilTileLbl, LABEL_RETINA_SIZE, LABEL_RETINA_SIZE);

         // Save the tile. 
         MIL_TEXT_CHAR Suffix[128];
         MosSprintf(Suffix, 128, MIL_TEXT("_Tile_%0.2d"), TileIndex);
         MIL_STRING TileFileName = DestPath + ClassNames[int(GroundTruth)] + MIL_TEXT("\\") + FileName;
         std::size_t DotPos = TileFileName.rfind(MIL_TEXT("."));
         TileFileName.insert(DotPos, Suffix);
         MbufSave(TileFileName, MilTileImg);

         // Add the saved tile to the dataset.
         MclassControl(DestDataset, M_DEFAULT, M_ENTRY_ADD, M_DEFAULT);
         MclassControlEntry(DestDataset, DstNbEntries + CurImageIndex, M_DEFAULT_KEY, M_REGION_INDEX(0), M_CLASS_INDEX_GROUND_TRUTH, GroundTruth, M_NULL, M_DEFAULT);
         MclassControlEntry(DestDataset, DstNbEntries + CurImageIndex, M_DEFAULT_KEY, M_DEFAULT, M_FILE_PATH, M_DEFAULT, TileFileName, M_DEFAULT);
         CurImageIndex++;
         }
      }

   MosPrintf(MIL_TEXT("\n"));
   }

void ExtractCoGTiles(MIL_ID MilSystem,
                     MIL_ID SourceDataset,
                     MIL_INT NbClasses,
                     MIL_INT TileSizeX,
                     MIL_INT TileSizeY,
                     MIL_STRING ImagesPath,
                     MIL_STRING LabelsPath,
                     MIL_STRING DestPath,
                     MIL_STRING* ClassNames,
                     MIL_ID DestDataset)
   {
   // Inquire the number of images already added to the datasets. 
   MIL_INT SrcNbEntries, DstNbEntries, CurImageIndex = 0;
   MclassInquire(SourceDataset, M_DEFAULT, M_NUMBER_OF_ENTRIES + M_TYPE_MIL_INT, &SrcNbEntries);
   MclassInquire(DestDataset, M_DEFAULT, M_NUMBER_OF_ENTRIES + M_TYPE_MIL_INT, &DstNbEntries);

   // Allocate blob analysis to locate the CoG of classes. 
   auto MilBlobCtx = MblobAlloc(MilSystem, M_DEFAULT, M_DEFAULT, M_UNIQUE_ID);
   auto MilBlobRslt = MblobAllocResult(MilSystem, M_DEFAULT, M_DEFAULT, M_UNIQUE_ID);
   MblobControl(MilBlobCtx, M_CENTER_OF_GRAVITY, M_ENABLE);

   // Iterate over all the entries.
   for(MIL_INT ind = 0; ind < SrcNbEntries; ind++)
      {
      MosPrintf(MIL_TEXT("   %d of %d completed\r"), ind + 1, SrcNbEntries);

      // Get the file names.
      MIL_STRING FileName, ImgPath, LblPath;
      MclassInquireEntry(SourceDataset, ind, M_DEFAULT_KEY, M_DEFAULT, M_FILE_PATH, FileName);
      ImgPath = ImagesPath + FileName;
      LblPath = LabelsPath + FileName;

      // Load the original image and the label image. 
      auto OriginalImage = MbufRestore(ImgPath, MilSystem, M_UNIQUE_ID);
      auto OriginalLabel = MbufRestore(LblPath, MilSystem, M_UNIQUE_ID);

      MIL_INT ImageSizeX = MbufInquire(OriginalImage, M_SIZE_X, M_NULL);
      MIL_INT ImageSizeY = MbufInquire(OriginalImage, M_SIZE_Y, M_NULL);
      MIL_INT ImageSizeBand = MbufInquire(OriginalImage, M_SIZE_BAND, M_NULL);

      // Allocate Binarized Label and the tile image. 
      auto MilBinLabel = MbufAlloc2d(MilSystem, ImageSizeX, ImageSizeY, 8 + M_UNSIGNED, M_IMAGE + M_PROC, M_UNIQUE_ID);
      auto MilTileImg  = MbufAllocColor(MilSystem, ImageSizeBand, TileSizeX, TileSizeY, 8 + M_UNSIGNED, M_IMAGE + M_PROC + M_DISP, M_UNIQUE_ID);
      auto MilTileLbl = MbufAllocColor(MilSystem, 1, TileSizeX, TileSizeY, 8 + M_UNSIGNED, M_IMAGE + M_PROC + M_DISP, M_UNIQUE_ID);

      MIL_INT NbBlobs;
      std::vector<MIL_INT> CentersX;
      std::vector<MIL_INT> CentersY;

      // Iterate over all the classes except class 0 since in this example 0 is the background. 
      for(int LabelIndex = 1; LabelIndex < NbClasses; LabelIndex++)
         {
         // Calculate the CoG for all the blobs. 
         MimBinarize(OriginalLabel, MilBinLabel, M_FIXED + M_EQUAL, LabelIndex, M_NULL);
         MblobCalculate(MilBlobCtx, MilBinLabel, M_NULL, MilBlobRslt);
         MblobGetResult(MilBlobRslt, M_DEFAULT, M_NUMBER + M_TYPE_MIL_INT, &NbBlobs);

         CentersX.resize(NbBlobs);
         CentersY.resize(NbBlobs);

         MblobGetResult(MilBlobRslt, M_DEFAULT, M_CENTER_OF_GRAVITY_X, CentersX);
         MblobGetResult(MilBlobRslt, M_DEFAULT, M_CENTER_OF_GRAVITY_Y, CentersY);

         // Iterate over all the blobs.
         for(int TileIndex = 0; TileIndex < NbBlobs; TileIndex++)
            {
            // The tile should reside inside the image. 
            MIL_INT OffsetX = std::max<MIL_INT>(0, CentersX[TileIndex] - TileSizeX / 2);
            MIL_INT OffsetY = std::max<MIL_INT>(0, CentersY[TileIndex] - TileSizeY / 2);
            OffsetX = std::min<MIL_INT>(OffsetX, ImageSizeX - TileSizeX);
            OffsetY = std::min<MIL_INT>(OffsetY, ImageSizeY - TileSizeY);

            // Clear the destination and copy the data. 
            MbufClear(MilTileImg, M_COLOR_BLACK);
            MbufCopyColor2d(OriginalImage, MilTileImg, M_ALL_BANDS, OffsetX, OffsetY, M_ALL_BANDS, 0, 0, TileSizeX, TileSizeY);

            // Clear the destination and copy the label. 
            MbufClear(MilTileLbl, M_COLOR_BLACK);
            MbufCopyColor2d(OriginalLabel, MilTileLbl, M_ALL_BANDS, OffsetX, OffsetY, M_ALL_BANDS, 0, 0, TileSizeX, TileSizeY);

            // To check if the defect is not next to the border and the defects dont overlap. 
            MIL_DOUBLE RetinaLabel = GetRetinaLabel(MilSystem, MilTileLbl, (MIL_INT) (TILE_IMAGE_SIZE * 0.8), (MIL_INT) (TILE_IMAGE_SIZE * 0.8));
            if(RetinaLabel == LabelIndex)
               {
               // Save the extraced tile. 
               MIL_TEXT_CHAR Suffix[128];
               MosSprintf(Suffix, 128, MIL_TEXT("_CoG_%0.2d_%0.2d"), LabelIndex, TileIndex);
               MIL_STRING TileFileName = DestPath + ClassNames[LabelIndex] + MIL_TEXT("\\") + FileName;
               std::size_t DotPos = TileFileName.rfind(MIL_TEXT("."));
               TileFileName.insert(DotPos, Suffix);
               MbufSave(TileFileName, MilTileImg);

               // Add to dataset.
               MclassControl(DestDataset, M_DEFAULT, M_ENTRY_ADD, M_DEFAULT);
               MclassControlEntry(DestDataset, DstNbEntries + CurImageIndex, M_DEFAULT_KEY, M_REGION_INDEX(0), M_CLASS_INDEX_GROUND_TRUTH, LabelIndex, M_NULL, M_DEFAULT);
               MclassControlEntry(DestDataset, DstNbEntries + CurImageIndex, M_DEFAULT_KEY, M_DEFAULT, M_FILE_PATH, M_DEFAULT, TileFileName, M_DEFAULT);
               CurImageIndex++;
               }
            }
         }
      }

   MosPrintf(MIL_TEXT("\n"));
   }

// Uses a retina box to decide the label of a tile.
MIL_DOUBLE GetRetinaLabel(MIL_ID MilSystem, MIL_ID LabelImage, MIL_INT RetinaSizeX, MIL_INT RetinaSizeY)
   {
   MIL_DOUBLE LabelValue;
   MIL_INT SizeX, SizeY;

   MbufInquire(LabelImage, M_SIZE_X, &SizeX);
   MbufInquire(LabelImage, M_SIZE_Y, &SizeY);

   MIL_INT OffsetX = (SizeX - RetinaSizeX) / 2;
   MIL_INT OffsetY = (SizeY - RetinaSizeY) / 2;

   auto MilRetinaImg = MbufChild2d(LabelImage, OffsetX, OffsetY, RetinaSizeX, RetinaSizeY, M_UNIQUE_ID);

   // In this example, if there are multiple label values in the retina box, 
   // we use the max value as the winner.
   auto MilStatContext = MimAlloc(MilSystem, M_STATISTICS_CONTEXT, M_DEFAULT, M_UNIQUE_ID);
   auto MilStatResult = MimAllocResult(MilSystem, M_DEFAULT, M_STATISTICS_RESULT, M_UNIQUE_ID);

   MimControl(MilStatContext, M_STAT_MAX, M_ENABLE);
   MimStatCalculate(MilStatContext, MilRetinaImg, MilStatResult, M_DEFAULT);
   MimGetResult(MilStatResult, M_STAT_MAX, &LabelValue);

   return LabelValue;
   }

MIL_STRING GetExampleCurrentDirectory()
   {
   DWORD CurDirStrSize = GetCurrentDirectory(0, NULL) + 1;

   std::vector<MIL_TEXT_CHAR> vCurDir(CurDirStrSize, 0);
   GetCurrentDirectory(CurDirStrSize, (LPTSTR)&vCurDir[0]);

   MIL_STRING sRet = &vCurDir[0];
   return sRet;
   }

MIL_UNIQUE_BUF_ID CreateImageOfAllClasses(MIL_ID MilSystem, const MIL_STRING* ClassIcons, const MIL_STRING* ClassNames, MIL_INT NumberOfClasses)
   {
   MIL_INT MaxSizeY = MIL_INT_MIN;
   MIL_INT SumSizeX = 0;
   std::vector<MIL_UNIQUE_BUF_ID> IconsToDisplay;
   for(MIL_INT i = 0; i < NumberOfClasses; i++)
      {
      IconsToDisplay.push_back(MbufRestore(ClassIcons[i], MilSystem, M_UNIQUE_ID));
      MIL_INT SizeX = MbufInquire(IconsToDisplay.back(), M_SIZE_X, M_NULL);
      MIL_INT SizeY = MbufInquire(IconsToDisplay.back(), M_SIZE_Y, M_NULL);

      MaxSizeY = std::max<MIL_INT>(SizeY, MaxSizeY);
      SumSizeX = SumSizeX + SizeX;
      }

   MIL_UNIQUE_BUF_ID AllClassesImage = MbufAllocColor(MilSystem, 3, SumSizeX, MaxSizeY, 8 + M_UNSIGNED, M_IMAGE + M_PROC + M_DISP, M_UNIQUE_ID);
   MbufClear(AllClassesImage, 0.0);

   MIL_UNIQUE_GRA_ID GraContext = MgraAlloc(MilSystem, M_UNIQUE_ID);
   const MIL_INT TEXT_MARGIN = 2;
   MIL_INT CurXOffset = 0;
   for(MIL_INT i = 0; i < NumberOfClasses; i++)
      {
      MIL_ID IconImage = IconsToDisplay[i];
      MIL_INT SizeX = MbufInquire(IconImage, M_SIZE_X, M_NULL);
      MIL_INT SizeY = MbufInquire(IconImage, M_SIZE_Y, M_NULL);

      MbufCopyColor2d(IconImage, AllClassesImage, M_ALL_BANDS, 0, 0, M_ALL_BANDS, CurXOffset, 0, SizeX, SizeY);
      MgraColor(GraContext, M_COLOR_BLUE);
      MgraRect(GraContext, AllClassesImage, CurXOffset, 0, CurXOffset + SizeX - 1, SizeY - 1);
      MgraColor(GraContext, M_COLOR_LIGHT_BLUE);
      MgraText(GraContext, AllClassesImage, CurXOffset + TEXT_MARGIN, TEXT_MARGIN, ClassNames[i]);
      CurXOffset += SizeX;
      }

   return AllClassesImage;
   }

const std::vector<MIL_INT> CreateShuffledIndex(MIL_INT NbEntries, unsigned int Seed)
   {
   std::vector<MIL_INT> IndexVector(NbEntries);
   std::iota(IndexVector.begin(), IndexVector.end(), 0);
   std::mt19937 gen(Seed);
   std::shuffle(IndexVector.begin(), IndexVector.end(), gen);
   return IndexVector;
   }

void DeleteFiles(const std::vector<MIL_STRING>& Files)
   {
   for(const auto& FileName : Files)
      {
      MappFileOperation(M_DEFAULT, FileName, M_NULL, M_NULL, M_FILE_DELETE, M_DEFAULT, M_NULL);
      }
   }

void ListFilesInFolder(const MIL_ID MilApplication, const MIL_STRING& FolderName, std::vector<MIL_STRING>& FilesInFolder)
   {
   MIL_STRING FileToSearch = FolderName;
   FileToSearch += MIL_TEXT("*.bmp");

   MIL_INT NumberOfFiles;
   MappFileOperation(MilApplication, FileToSearch, M_NULL, M_NULL, M_FILE_NAME_FIND_COUNT, M_DEFAULT, &NumberOfFiles);
   FilesInFolder.resize(NumberOfFiles);

   std::vector<MIL_TEXT_CHAR> vFilename;
   vFilename.reserve(260);
   for (MIL_INT i = 0; i < NumberOfFiles; i++)
      {
      MIL_INT FilenameStrSize = 0;
      MappFileOperation(MilApplication, FileToSearch, M_NULL, M_NULL, M_FILE_NAME_FIND + M_STRING_SIZE, i, &FilenameStrSize);

      vFilename.resize(FilenameStrSize);
      MappFileOperation(MilApplication, FileToSearch, M_NULL, M_NULL, M_FILE_NAME_FIND, i, &vFilename[0]);
      FilesInFolder[i] = FolderName + &vFilename[0];
      }
   }

void AddClassDefinitions(MIL_ID MilSystem,
                         MIL_ID Dataset,
                         const MIL_STRING* ClassName,
                         const MIL_STRING* ClassIcon,
                         MIL_INT NumberOfClasses)
   {
   for(MIL_INT i = 0; i < NumberOfClasses; i++)
      {
      MclassControl(Dataset, M_DEFAULT, M_CLASS_ADD, ClassName[i]);
      MIL_UNIQUE_BUF_ID IconImageId = MbufRestore(ClassIcon[i], MilSystem, M_UNIQUE_ID);
      MclassControl(Dataset, M_CLASS_INDEX(i), M_CLASS_ICON_ID, IconImageId.get());
      }
   }

void DeleteFilesInFolder(const MIL_ID MilApplication, const MIL_STRING& FolderName)
   {
   std::vector<MIL_STRING> FilesInFolder;
   ListFilesInFolder(MilApplication, FolderName, FilesInFolder);
   DeleteFiles(FilesInFolder);
   }

// Create the required directories.
void PrepareExampleDataFolder(const MIL_ID MilApplication, const MIL_STRING& ExampleDataPath, const MIL_STRING* ClassName, MIL_INT NumberOfClasses)
   {
   MIL_INT FileExists;
   MappFileOperation(M_DEFAULT, ExampleDataPath, M_NULL, M_NULL, M_FILE_EXISTS, M_DEFAULT, &FileExists);

   if(FileExists != M_YES)
      {
      MosPrintf(MIL_TEXT("\nCreating the %s folder and a sub folder for each class"), ExampleDataPath.c_str());

      // Create ExampleDataPath folder since it does not exist.
      MappFileOperation(M_DEFAULT, ExampleDataPath, M_NULL, M_NULL, M_FILE_MAKE_DIR, M_DEFAULT, M_NULL);
      for(MIL_INT i = 0; i < NumberOfClasses; i++)
         {
         MosPrintf(MIL_TEXT("."));

         // Create one folder for each class name.
         MappFileOperation(M_DEFAULT, ExampleDataPath + ClassName[i], M_NULL, M_NULL, M_FILE_MAKE_DIR, M_DEFAULT, M_NULL);
         }
      MosPrintf(MIL_TEXT("\n"));
      }
   else
      {
      // If ExampleDataPath folder is existing, delete files already in there
      // Create the folder if not existing.
      MosPrintf(MIL_TEXT("\nDeleting files in the %s folder to ensure example repeatability"), ExampleDataPath.c_str());

      for(MIL_INT i = 0; i < NumberOfClasses; i++)
         {
         MosPrintf(MIL_TEXT("."));

         MappFileOperation(M_DEFAULT, ExampleDataPath + ClassName[i], M_NULL, M_NULL, M_FILE_EXISTS, M_DEFAULT, &FileExists);
         if(FileExists)
            DeleteFilesInFolder(MilApplication, ExampleDataPath + ClassName[i] + MIL_TEXT("/"));
         else
            MappFileOperation(M_DEFAULT, ExampleDataPath + ClassName[i], M_NULL, M_NULL, M_FILE_MAKE_DIR, M_DEFAULT, M_NULL);
         }
      MosPrintf(MIL_TEXT("\n"));
      }
   }

void AddFolderToDataset(const MIL_ID MilApplication, const MIL_STRING& DataPath, MIL_ID Dataset)
   {
   MIL_INT NbEntries;
   MclassInquire(Dataset, M_DEFAULT, M_NUMBER_OF_ENTRIES + M_TYPE_MIL_INT, &NbEntries);

   std::vector<MIL_STRING> FilesInFolder;
   ListFilesInFolder(MilApplication, DataPath, FilesInFolder);

   MIL_INT CurImageIndex = 0;
   for(const auto& File : FilesInFolder)
      {
      MIL_STRING fileLocalPath = File.substr(DataPath.length());
      MclassControl(Dataset, M_DEFAULT, M_ENTRY_ADD, M_DEFAULT);
      MclassControlEntry(Dataset, NbEntries + CurImageIndex, M_DEFAULT_KEY, M_DEFAULT, M_FILE_PATH, M_DEFAULT, fileLocalPath, M_DEFAULT);
      MclassControlEntry(Dataset, NbEntries + CurImageIndex, M_DEFAULT_KEY, M_REGION_INDEX(0), M_CLASS_INDEX_GROUND_TRUTH, 0, M_NULL, M_DEFAULT);
      CurImageIndex++;
      }
   }

void AugmentDataset(MIL_ID System, MIL_ID Dataset, const MIL_INT* NbAugmentPerImage)
   {
   auto AugmentContext = MimAlloc(System, M_AUGMENTATION_CONTEXT, M_DEFAULT, M_UNIQUE_ID);
   auto AugmentResult = MimAllocResult(System, M_DEFAULT, M_AUGMENTATION_RESULT, M_UNIQUE_ID);

   // Seed the augmentation to ensure repeatability.
   MimControl(AugmentContext, M_AUG_SEED_MODE, M_RNG_INIT_VALUE);
   MimControl(AugmentContext, M_AUG_RNG_INIT_VALUE, 42);

   MimControl(AugmentContext, M_AUG_TRANSLATION_X_OP, M_ENABLE);
   MimControl(AugmentContext, M_AUG_TRANSLATION_Y_OP, M_ENABLE);
   MimControl(AugmentContext, M_AUG_TRANSLATION_X_OP_MAX, 5);
   MimControl(AugmentContext, M_AUG_TRANSLATION_Y_OP_MAX, 5);

   MimControl(AugmentContext, M_AUG_SCALE_OP, M_ENABLE);
   MimControl(AugmentContext, M_AUG_SCALE_OP_FACTOR_MIN, 0.95);
   MimControl(AugmentContext, M_AUG_SCALE_OP_FACTOR_MAX, 1.05);

   MimControl(AugmentContext, M_AUG_ASPECT_RATIO_OP, M_ENABLE);
   MimControl(AugmentContext, M_AUG_ASPECT_RATIO_OP + M_PROBABILITY, 75);
   MimControl(AugmentContext, M_AUG_ASPECT_RATIO_OP_MODE, M_BOTH);
   MimControl(AugmentContext, M_AUG_ASPECT_RATIO_OP_MIN, 0.95);
   MimControl(AugmentContext, M_AUG_ASPECT_RATIO_OP_MAX, 1.05);

   MimControl(AugmentContext, M_AUG_ROTATION_OP, M_ENABLE);
   MimControl(AugmentContext, M_AUG_ROTATION_OP_ANGLE_DELTA, 5.0);

   MimControl(AugmentContext, M_AUG_FLIP_OP, M_ENABLE);
   MimControl(AugmentContext, M_AUG_FLIP_OP + M_PROBABILITY, 70);
   MimControl(AugmentContext, M_AUG_FLIP_OP_DIRECTION, M_BOTH);

   MimControl(AugmentContext, M_AUG_INTENSITY_ADD_OP, M_ENABLE);
   MimControl(AugmentContext, M_AUG_INTENSITY_ADD_OP_MODE, M_LUMINANCE);
   MimControl(AugmentContext, M_AUG_INTENSITY_ADD_OP_DELTA, 30.0);

   MimControl(AugmentContext, M_AUG_NOISE_GAUSSIAN_ADDITIVE_OP, M_ENABLE);
   MimControl(AugmentContext, M_AUG_NOISE_GAUSSIAN_ADDITIVE_OP + M_PROBABILITY, 25);
   MimControl(AugmentContext, M_AUG_NOISE_GAUSSIAN_ADDITIVE_OP_STDDEV, 0.005);
   MimControl(AugmentContext, M_AUG_NOISE_GAUSSIAN_ADDITIVE_OP_STDDEV_DELTA, 0.005);

   MIL_INT NbEntries = 0;
   MclassInquire(Dataset, M_DEFAULT, M_NUMBER_OF_ENTRIES + M_TYPE_MIL_INT, &NbEntries);

   MIL_INT PosInAugmentDataset = NbEntries;

   for(MIL_INT i = 0; i < NbEntries; i++)
      {
      MosPrintf(MIL_TEXT("   %d of %d completed\r"), i + 1, NbEntries);

      MIL_STRING FilePath;
      MclassInquireEntry(Dataset, i, M_DEFAULT_KEY, M_DEFAULT, M_FILE_PATH, FilePath);
      MIL_INT GroundTruthIndex;
      MclassInquireEntry(Dataset, i, M_DEFAULT_KEY, M_REGION_INDEX(0), M_CLASS_INDEX_GROUND_TRUTH + M_TYPE_MIL_INT, &GroundTruthIndex);

      // Add the augmentations.
      MIL_UNIQUE_BUF_ID OrginalImage = MbufRestore(FilePath, System, M_UNIQUE_ID);
      MIL_UNIQUE_BUF_ID AugmentedImage = MbufClone(OrginalImage, M_DEFAULT, M_DEFAULT, M_DEFAULT, M_DEFAULT, M_DEFAULT, M_DEFAULT, M_UNIQUE_ID);
      for(MIL_INT AugIndex = 0; AugIndex < NbAugmentPerImage[GroundTruthIndex]; AugIndex++)
         {
         MbufClear(AugmentedImage, 0.0);
         MimAugment(AugmentContext, OrginalImage, AugmentedImage, M_DEFAULT, M_DEFAULT);

         MIL_TEXT_CHAR Suffix[128];
         MosSprintf(Suffix, 128, MIL_TEXT("_Aug_%d"), AugIndex);

         MIL_STRING AugFileName = FilePath;
         std::size_t DotPos = AugFileName.rfind(MIL_TEXT("."));
         AugFileName.insert(DotPos, Suffix);
         MbufSave(AugFileName, AugmentedImage);

         // Add the augmented image.
         MclassControl(Dataset, M_DEFAULT, M_ENTRY_ADD, M_DEFAULT);
         MclassControlEntry(Dataset, PosInAugmentDataset, M_DEFAULT_KEY, M_REGION_INDEX(0), M_CLASS_INDEX_GROUND_TRUTH, GroundTruthIndex, M_NULL, M_DEFAULT);
         MclassControlEntry(Dataset, PosInAugmentDataset, M_DEFAULT_KEY, M_DEFAULT, M_FILE_PATH, M_DEFAULT, AugFileName, M_DEFAULT);

         // Identify the fact that this is augmented data in case we want to use this dataset later.
         MclassControlEntry(Dataset, PosInAugmentDataset, M_DEFAULT_KEY, M_DEFAULT, M_AUGMENTATION_SOURCE, i, M_NULL, M_DEFAULT);

         PosInAugmentDataset++;
         }
      }
   MosPrintf(MIL_TEXT("\n"));
   }

void CropDatasetImages(MIL_ID MilSystem, MIL_ID Dataset, MIL_INT FinalImageSize)
   {
   MIL_INT NbEntries;
   MclassInquire(Dataset, M_DEFAULT, M_NUMBER_OF_ENTRIES + M_TYPE_MIL_INT, &NbEntries);

   for(MIL_INT i = 0; i < NbEntries; i++)
      {
      MosPrintf(MIL_TEXT("   %d of %d completed\r"), i + 1, NbEntries);

      MIL_STRING FilePath;
      MclassInquireEntry(Dataset, i, M_DEFAULT_KEY, M_DEFAULT, M_FILE_PATH, FilePath);

      MIL_UNIQUE_BUF_ID OriginalImage = MbufRestore(FilePath, MilSystem, M_UNIQUE_ID);

      MIL_INT ImageSizeX = MbufInquire(OriginalImage, M_SIZE_X, M_NULL);
      MIL_INT ImageSizeY = MbufInquire(OriginalImage, M_SIZE_Y, M_NULL);

      // We crop by taking the centered pixels.
      MIL_INT OffsetX = (ImageSizeX - FinalImageSize) / 2;
      MIL_INT OffsetY = (ImageSizeY - FinalImageSize) / 2;

      MIL_UNIQUE_BUF_ID CroppedImage = MbufClone(OriginalImage, M_DEFAULT, FinalImageSize, FinalImageSize, M_DEFAULT, M_DEFAULT, M_DEFAULT, M_UNIQUE_ID);

      MbufCopyColor2d(OriginalImage, CroppedImage, M_ALL_BANDS, OffsetX, OffsetY, M_ALL_BANDS, 0, 0, FinalImageSize, FinalImageSize);

      MbufSave(FilePath, CroppedImage);
      }

   MosPrintf(MIL_TEXT("\n"));
   }
