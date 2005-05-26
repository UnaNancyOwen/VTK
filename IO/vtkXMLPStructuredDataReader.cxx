/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkXMLPStructuredDataReader.cxx

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkXMLPStructuredDataReader.h"

#include "vtkDataArray.h"
#include "vtkDataSet.h"
#include "vtkExtentSplitter.h"
#include "vtkTableExtentTranslator.h"
#include "vtkXMLDataElement.h"
#include "vtkXMLStructuredDataReader.h"

vtkCxxRevisionMacro(vtkXMLPStructuredDataReader, "1.19");

//----------------------------------------------------------------------------
vtkXMLPStructuredDataReader::vtkXMLPStructuredDataReader()
{
  this->ExtentTranslator = vtkTableExtentTranslator::New();
  this->ExtentSplitter = vtkExtentSplitter::New();
  this->PieceExtents = 0;
}

//----------------------------------------------------------------------------
vtkXMLPStructuredDataReader::~vtkXMLPStructuredDataReader()
{
  if(this->NumberOfPieces) { this->DestroyPieces(); }
  this->ExtentSplitter->Delete();
  this->ExtentTranslator->Delete();
}

//----------------------------------------------------------------------------
void vtkXMLPStructuredDataReader::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//----------------------------------------------------------------------------
vtkExtentTranslator* vtkXMLPStructuredDataReader::GetExtentTranslator()
{
  return this->ExtentTranslator;
}

//----------------------------------------------------------------------------
vtkIdType vtkXMLPStructuredDataReader::GetNumberOfPoints()
{
  return (this->PointDimensions[0]*
          this->PointDimensions[1]*
          this->PointDimensions[2]);
}

//----------------------------------------------------------------------------
vtkIdType vtkXMLPStructuredDataReader::GetNumberOfCells()
{
  return (this->CellDimensions[0]*
          this->CellDimensions[1]*
          this->CellDimensions[2]);
}

//----------------------------------------------------------------------------
void vtkXMLPStructuredDataReader::ReadXMLData()
{
  // Get the requested Update Extent.
  this->GetOutputAsDataSet(0)->GetUpdateExtent(this->UpdateExtent);
  
  vtkDebugMacro("Updating extent "
                << this->UpdateExtent[0] << " " << this->UpdateExtent[1] << " "
                << this->UpdateExtent[2] << " " << this->UpdateExtent[3] << " "
                << this->UpdateExtent[4] << " " << this->UpdateExtent[5]
                << "\n");  
  
  // Prepare increments for the update extent.
  this->ComputeDimensions(this->UpdateExtent, this->PointDimensions, 1);
  this->ComputeIncrements(this->UpdateExtent, this->PointIncrements, 1);
  this->ComputeDimensions(this->UpdateExtent, this->CellDimensions, 0);
  this->ComputeIncrements(this->UpdateExtent, this->CellIncrements, 0);  
  
  // Let superclasses read data.  This also allocates output data.
  this->Superclass::ReadXMLData();
  
  // Use the ExtentSplitter to split the update extent into
  // sub-extents read by each piece.
  if(!this->ComputePieceSubExtents())
    {
    // Not all needed data are available.
    this->DataError = 1;
    return;
    }
  
  // Split current progress range based on fraction contributed by
  // each sub-extent.
  float progressRange[2] = {0,0};
  this->GetProgressRange(progressRange);
  
  // Calculate the cumulative fraction of data contributed by each
  // sub-extent (for progress).
  int n = this->ExtentSplitter->GetNumberOfSubExtents();
  float* fractions = new float[n+1];
  int i;
  fractions[0] = 0;
  for(i=0;i < n;++i)
    {
    // Get this sub-extent.
    this->ExtentSplitter->GetSubExtent(i, this->SubExtent);
    
    // Add this sub-extent's volume to the cumulative volume.
    int pieceDims[3] = {0,0,0};
    this->ComputeDimensions(this->SubExtent, pieceDims, 1);
    fractions[i+1] = fractions[i] + pieceDims[0]*pieceDims[1]*pieceDims[2];
    }
  if(fractions[n] == 0)
    {
    fractions[n] = 1;
    }
  for(i=1;i <= n;++i)
    {
    fractions[i] = fractions[i] / fractions[n];
    }
  
  // Read the data needed from each sub-extent.
  for(i=0;(i < n && !this->AbortExecute && !this->DataError);++i)
    {
    // Set the range of progress for this sub-extent.
    this->SetProgressRange(progressRange, i, fractions);
    
    // Get this sub-extent and the piece from which to read it.
    int piece = this->ExtentSplitter->GetSubExtentSource(i);
    this->ExtentSplitter->GetSubExtent(i, this->SubExtent);
    
    vtkDebugMacro("Reading extent "
                  << this->SubExtent[0] << " " << this->SubExtent[1] << " "
                  << this->SubExtent[2] << " " << this->SubExtent[3] << " "
                  << this->SubExtent[4] << " " << this->SubExtent[5]
                  << " from piece " << piece);
    
    this->ComputeDimensions(this->SubExtent, this->SubPointDimensions, 1);
    this->ComputeDimensions(this->SubExtent, this->SubCellDimensions, 0);
    
    // Read the data from this piece.
    if(!this->Superclass::ReadPieceData(piece))
      {
      // An error occurred while reading the piece.
      this->DataError = 1;
      }
    }
  
  delete [] fractions;
  
  // We filled the exact update extent in the output.
  this->SetOutputExtent(this->UpdateExtent);  
}

//----------------------------------------------------------------------------
int
vtkXMLPStructuredDataReader::ReadPrimaryElement(vtkXMLDataElement* ePrimary)
{
  if(!this->Superclass::ReadPrimaryElement(ePrimary)) { return 0; }
  vtkDataSet* output = this->GetOutputAsDataSet(0);
  
  // Read information about the structured data.
  int extent[6];
  if(ePrimary->GetVectorAttribute("WholeExtent", extent) < 6)
    {
    vtkErrorMacro(<< this->GetDataSetName()
                  << " element has no WholeExtent.");
    return 0;
    }
  output->SetWholeExtent(extent);  
  
  return 1;
}


void vtkXMLPStructuredDataReader::SetupOutputData()
  {
  this->Superclass::SetupOutputData();

  // Tell the output to use the table extent translator to provide the
  // correct piece breakdown for the file layout.
  this->GetOutputAsDataSet(0)->SetExtentTranslator(this->ExtentTranslator);
  }


//----------------------------------------------------------------------------
void vtkXMLPStructuredDataReader::SetupEmptyOutput()
{
  // Special extent to indicate no input.
  this->GetOutputAsDataSet(0)->SetUpdateExtent(1, 0, 1, 0, 1, 0);
}

//----------------------------------------------------------------------------
void vtkXMLPStructuredDataReader::SetupPieces(int numPieces)
{
  this->Superclass::SetupPieces(numPieces);
  this->ExtentTranslator->SetNumberOfPiecesInTable(this->NumberOfPieces);
  this->ExtentTranslator->SetMaximumGhostLevel(this->GhostLevel);
  this->PieceExtents = new int[6*this->NumberOfPieces];
  int i;
  for(i=0;i < this->NumberOfPieces;++i)
    {
    int* extent = this->PieceExtents+i*6;
    extent[0] = 0; extent[1] = -1;
    extent[2] = 0; extent[3] = -1;
    extent[4] = 0; extent[5] = -1;
    }
}

//----------------------------------------------------------------------------
void vtkXMLPStructuredDataReader::DestroyPieces()
{
  delete [] this->PieceExtents;
  this->PieceExtents = 0;
  this->Superclass::DestroyPieces();
}

//----------------------------------------------------------------------------
int vtkXMLPStructuredDataReader::ReadPiece(vtkXMLDataElement* ePiece)
{
  // Superclass will create a reader for the piece's file.
  if(!this->Superclass::ReadPiece(ePiece)) { return 0; }
  
  // Get the extent of the piece.
  int* pieceExtent = this->PieceExtents+this->Piece*6;
  if(ePiece->GetVectorAttribute("Extent", pieceExtent) < 6)
    {
    vtkErrorMacro("Piece " << this->Piece << " has invalid Extent.");
    return 0;
    }
  
  // Set this table entry in the extent translator.
  this->ExtentTranslator->SetExtentForPiece(this->Piece, pieceExtent);
  this->ExtentTranslator->SetPieceAvailable(this->Piece,
                                            this->CanReadPiece(this->Piece));
  
  return 1;
}

//----------------------------------------------------------------------------
int vtkXMLPStructuredDataReader::ReadPieceData()
{  
  // Use the internal reader to read the piece.
  vtkDataSet* input = this->GetPieceInputAsDataSet(this->Piece);
  input->SetUpdateExtent(this->SubExtent);
  input->Update();
  
  // Skip rest of read if aborting.
  if(this->AbortExecute)
    {
    return 0;
    }
  
  // Get the actual portion of the piece that was read.
  this->GetPieceInputExtent(this->Piece, this->SubPieceExtent);
  this->ComputeDimensions(this->SubPieceExtent,
                          this->SubPiecePointDimensions, 1);
  this->ComputeIncrements(this->SubPieceExtent,
                          this->SubPiecePointIncrements, 1);
  this->ComputeDimensions(this->SubPieceExtent,
                          this->SubPieceCellDimensions, 0);
  this->ComputeIncrements(this->SubPieceExtent,
                          this->SubPieceCellIncrements, 0);
  
  // Let the superclass read the data it wants.
  return this->Superclass::ReadPieceData();
}

//----------------------------------------------------------------------------
void vtkXMLPStructuredDataReader::CopyArrayForPoints(vtkDataArray* inArray,
                                                     vtkDataArray* outArray)
{
  if(!inArray || !outArray)
    {
    return;
    }
  this->CopySubExtent(this->SubPieceExtent,
                      this->SubPiecePointDimensions,
                      this->SubPiecePointIncrements,
                      this->UpdateExtent, this->PointDimensions,
                      this->PointIncrements, this->SubExtent,
                      this->SubPointDimensions, inArray, outArray);
}

//----------------------------------------------------------------------------
void vtkXMLPStructuredDataReader::CopyArrayForCells(vtkDataArray* inArray,
                                                    vtkDataArray* outArray)
{
  if(!inArray || !outArray)
    {
    return;
    }
  this->CopySubExtent(this->SubPieceExtent,
                      this->SubPieceCellDimensions,
                      this->SubPieceCellIncrements,
                      this->UpdateExtent, this->CellDimensions,
                      this->CellIncrements, this->SubExtent,
                      this->SubCellDimensions, inArray, outArray);
}

//----------------------------------------------------------------------------
void
vtkXMLPStructuredDataReader
::CopySubExtent(int* inExtent, int* inDimensions, vtkIdType* inIncrements,
                int* outExtent, int* outDimensions, vtkIdType* outIncrements,
                int* subExtent, int* subDimensions,
                vtkDataArray* inArray, vtkDataArray* outArray)
{
  unsigned int components = inArray->GetNumberOfComponents();
  unsigned int tupleSize = inArray->GetDataTypeSize()*components;
  
  if((inDimensions[0] == outDimensions[0]) &&
     (inDimensions[1] == outDimensions[1]))
    {
    if(inDimensions[2] == outDimensions[2])
      {
      // Copy the whole volume at once.
      unsigned int volumeTuples = (inDimensions[0]*
                                   inDimensions[1]*
                                   inDimensions[2]);
      memcpy(outArray->GetVoidPointer(0), inArray->GetVoidPointer(0),
             volumeTuples*tupleSize);
      }
    else
      {
      // Copy an entire slice at a time.
      vtkIdType sliceTuples = inDimensions[0]*inDimensions[1];
      int k;
      for(k=0;k < subDimensions[2];++k)
        {
        vtkIdType sourceTuple = this->GetStartTuple(inExtent, inIncrements,
                                                    subExtent[0],
                                                    subExtent[2],
                                                    subExtent[4]+k);
        vtkIdType destTuple = this->GetStartTuple(outExtent, outIncrements,
                                                  subExtent[0],
                                                  subExtent[2],
                                                  subExtent[4]+k);
        memcpy(outArray->GetVoidPointer(destTuple*components),
               inArray->GetVoidPointer(sourceTuple*components),
               sliceTuples*tupleSize);
        }
      }
    }
  else
    {
    // Copy a row at a time.
    vtkIdType rowTuples = subDimensions[0];
    int j,k;
    for(k=0;k < subDimensions[2];++k)
      {
      for(j=0;j < subDimensions[1];++j)
        {
        vtkIdType sourceTuple = this->GetStartTuple(inExtent, inIncrements,
                                                    subExtent[0],
                                                    subExtent[2]+j,
                                                    subExtent[4]+k);
        vtkIdType destTuple = this->GetStartTuple(outExtent, outIncrements,
                                                  subExtent[0],
                                                  subExtent[2]+j,
                                                  subExtent[4]+k);
        memcpy(outArray->GetVoidPointer(destTuple*components),
               inArray->GetVoidPointer(sourceTuple*components),
               rowTuples*tupleSize);
        }
      }
    }
}

//----------------------------------------------------------------------------
int vtkXMLPStructuredDataReader::ComputePieceSubExtents()
{
  // Reset the extent splitter.
  this->ExtentSplitter->RemoveAllExtentSources();
  
  // Add each readable piece as an extent source.
  int i;
  for(i=0;i < this->NumberOfPieces;++i)
    {
    if(this->CanReadPiece(i))
      {
      // Add the exact extent provided by the piece to the splitter.
      int extent[6];
      this->PieceReaders[i]->UpdateInformation();
      this->PieceReaders[i]->GetOutputAsDataSet()->GetWholeExtent(extent);
      this->ExtentSplitter->AddExtentSource(i, 0, extent);
      }
    }
  
  // We want to split the entire update extent across the pieces.
  this->ExtentSplitter->AddExtent(this->UpdateExtent);
  
  // Compute the sub-extents.
  if(!this->ExtentSplitter->ComputeSubExtents())
    {
    // A portion of the extent is not available.
    ostrstream e_with_warning_C4701;
    e_with_warning_C4701
      << "No available piece provides data for the following extents:\n";    
    for(i=0; i < this->ExtentSplitter->GetNumberOfSubExtents(); ++i)
      {
      if(this->ExtentSplitter->GetSubExtentSource(i) < 0)
        {
        int extent[6];
        this->ExtentSplitter->GetSubExtent(i, extent);
        e_with_warning_C4701
          << "    "
          << extent[0] << " " << extent[1] << "  "
          << extent[2] << " " << extent[3] << "  "
          << extent[4] << " " << extent[5] << "\n";
        }
      }
    e_with_warning_C4701 << "The UpdateExtent cannot be filled." << ends;
    vtkErrorMacro(<< e_with_warning_C4701.str());
    e_with_warning_C4701.rdbuf()->freeze(0);
    return 0;
    }
  
  return 1;
}

