/*==================================================================

  Program:   Visualization Toolkit
  Module:    TestHyperTreeGridTernary3DContour.cxx

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

===================================================================*/
// .SECTION Thanks
// This test was written by Philippe Pebay, Kitware 2012
// This work was supported in part by Commissariat a l'Energie Atomique (CEA/DIF)

#include "vtkHyperTreeGridSource.h"

#include "vtkCamera.h"
#include "vtkPointData.h"
#include "vtkContourFilter.h"
#include "vtkNew.h"
#include "vtkOutlineFilter.h"
#include "vtkProperty.h"
#include "vtkPolyDataMapper.h"
#include "vtkRegressionTestImage.h"
#include "vtkRenderer.h"
#include "vtkRenderWindow.h"
#include "vtkRenderWindowInteractor.h"

int TestHyperTreeGridTernary3DContour( int argc, char* argv[] )
{
  // Hyper tree grid
  vtkNew<vtkHyperTreeGridSource> htGrid;
  int maxLevel = 5;
  htGrid->SetMaximumLevel( maxLevel );
  htGrid->SetGridSize( 3, 3, 2 );
  htGrid->SetGridScale( 1.5, 1., .7 );
  htGrid->SetDimension( 3 );
  htGrid->SetBranchFactor( 3 );
  htGrid->DualOn();
  htGrid->SetDescriptor( "RRR .R. .RR ..R ..R .R.|R.......................... ........................... ........................... .............R............. ....RR.RR........R......... .....RRRR.....R.RR......... ........................... ........................... ...........................|........................... ........................... ........................... ...RR.RR.......RR.......... ........................... RR......................... ........................... ........................... ........................... ........................... ........................... ........................... ........................... ............RRR............|........................... ........................... .......RR.................. ........................... ........................... ........................... ........................... ........................... ........................... ........................... ...........................|........................... ..........................." );

  // Outline
  vtkNew<vtkOutlineFilter> outline;
  outline->SetInputConnection( htGrid->GetOutputPort() );

  // Contour
  vtkNew<vtkContourFilter> contour;
  int nContours = 3;
  contour->SetNumberOfContours( nContours );
  contour->SetInputConnection( htGrid->GetOutputPort() );
  double resolution = ( maxLevel - 1 ) / ( nContours + 1. );
  double isovalue = resolution;
  for ( int i = 0; i < nContours; ++ i, isovalue += resolution )
    {
    contour->SetValue( i, isovalue );
    }

  contour->GenerateTrianglesOff();
  contour->Update();
  if( contour->GetOutput()->GetNumberOfPoints() != 547 )
    {
    return 1;
    }
  if( contour->GetOutput()->GetNumberOfCells() != 463 )
    {
    return 1;
    }
  contour->GenerateTrianglesOn();
  contour->Update();
  if( contour->GetOutput()->GetNumberOfPoints() != 547 )
    {
    return 1;
    }
  if( contour->GetOutput()->GetNumberOfCells() != 917 )
    {
    return 1;
    }

  vtkPolyData* pd = contour->GetOutput();

  // Mappers
  vtkNew<vtkPolyDataMapper> mapper1;
  mapper1->SetInputConnection( contour->GetOutputPort() );
  mapper1->SetScalarRange( pd->GetPointData()->GetScalars()->GetRange() );
  mapper1->SetResolveCoincidentTopologyToPolygonOffset();
  mapper1->SetResolveCoincidentTopologyPolygonOffsetParameters( 0, 1 );
  vtkNew<vtkPolyDataMapper> mapper2;
  mapper2->SetInputConnection( contour->GetOutputPort() );
  mapper2->ScalarVisibilityOff();
  mapper2->SetResolveCoincidentTopologyToPolygonOffset();
  mapper2->SetResolveCoincidentTopologyPolygonOffsetParameters( 1, 1 );
  vtkNew<vtkPolyDataMapper> mapper3;
  mapper3->SetInputConnection( outline->GetOutputPort() );
  mapper3->ScalarVisibilityOff();

  // Actors
  vtkNew<vtkActor> actor1;
  actor1->SetMapper( mapper1.GetPointer() );
  vtkNew<vtkActor> actor2;
  actor2->SetMapper( mapper2.GetPointer() );
  actor2->GetProperty()->SetRepresentationToWireframe();
  actor2->GetProperty()->SetColor( .7, .7, .7 );
  vtkNew<vtkActor> actor3;
  actor3->SetMapper( mapper3.GetPointer() );
  actor3->GetProperty()->SetColor( .1, .1, .1 );
  actor3->GetProperty()->SetLineWidth( 1 );

  // Camera
  double bd[6];
  pd->GetBounds( bd );
  vtkNew<vtkCamera> camera;
  camera->SetClippingRange( 1., 100. );
  camera->SetFocalPoint( pd->GetCenter() );
  camera->SetPosition( -.8 * bd[1], 2.1 * bd[3], -4.8 * bd[5] );

  // Renderer
  vtkNew<vtkRenderer> renderer;
  renderer->SetActiveCamera( camera.GetPointer() );
  renderer->SetBackground( 1., 1., 1. );
  renderer->AddActor( actor1.GetPointer() );
  renderer->AddActor( actor2.GetPointer() );
  renderer->AddActor( actor3.GetPointer() );

  // Render window
  vtkNew<vtkRenderWindow> renWin;
  renWin->AddRenderer( renderer.GetPointer() );
  renWin->SetSize( 400, 400 );
  renWin->SetMultiSamples( 0 );

  // Interactor
  vtkNew<vtkRenderWindowInteractor> iren;
  iren->SetRenderWindow( renWin.GetPointer() );

  // Render and test
  renWin->Render();

  int retVal = vtkRegressionTestImage( renWin.GetPointer() );
  if ( retVal == vtkRegressionTester::DO_INTERACTOR )
    {
    iren->Start();
    }

  return !retVal;
}