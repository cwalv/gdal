/******************************************************************************
 * $Id$
 *
 * Project:  Web Coordinate Transformation Service
 * Purpose:  cgi-bin mainline for WCTS Implementation
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdma <warmerdam@pobox.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ******************************************************************************
 *
 * $Log$
 * Revision 1.2  2003/03/05 22:11:10  warmerda
 * implement istransformable
 *
 * Revision 1.1  2003/03/05 21:07:31  warmerda
 * New
 *
 */

#include <assert.h>
#include "ogrsf_frmts.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_minixml.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                      WCTSEmitServiceException()                      */
/************************************************************************/

static void WCTSEmitServiceException( const char *pszMessage )

{
    /* printf( "Content-type: text/xml%c%c", 10, 10 ); */
    printf("Content-type: application/vnd.ogc.se_xml%c%c",10,10);

    printf( "<?xml version='1.0' encoding=\"%s\" standalone=\"no\" ?>\n",
            "ISO-8859-1" );

    printf("<!DOCTYPE ServiceExceptionReport SYSTEM \"http://www.digitalearth.gov/wmt/xml/exception_1_1_0.dtd\">\n");

    printf("<ServiceExceptionReport version=\"1.1.0\">\n");        
    printf("<ServiceException>\n");
    printf("%s\n", pszMessage ); /* this should likely be XML escaped */
    printf("</ServiceException>\n");
    printf("</ServiceExceptionReport>\n");

    exit( 1 );
}

/************************************************************************/
/*                       WCTSCollectKVPRequest()                        */
/*                                                                      */
/*      Build an XML tree representation of a request received in       */
/*      KVP format via QUERY_STRING.                                    */
/************************************************************************/

CPLXMLNode *WCTSCollectKVPRequest()

{
    WCTSEmitServiceException( "KVP not supported yet." );

    return NULL;
}

/************************************************************************/
/*                         WCTSCollectRequest()                         */
/*                                                                      */
/*      This function will return an XML document in CPLXMLNode tree    */
/*      format corresponding to the current request.  If an error       */
/*      occurs the function does not return.  GET KVP style requests    */
/*      are internally converted into XML format.                       */
/************************************************************************/

CPLXMLNode *WCTSCollectRequest()

{
    if( getenv("REQUEST_METHOD") == NULL )
        WCTSEmitServiceException( "REQUEST_METHOD not set." );

    if( EQUAL(getenv("REQUEST_METHOD"),"GET") )
        return WCTSCollectKVPRequest();

/* -------------------------------------------------------------------- */
/*      Read the body of the POST message into a buffer.                */
/* -------------------------------------------------------------------- */
    int nContentLength = 0;
    char *pszXML = NULL;

    if( getenv("CONTENT_LENGTH") != NULL )
    {
        nContentLength = atoi(getenv("CONTENT_LENGTH"));

        pszXML = (char *) CPLMalloc(nContentLength+1);
        
        if( (int) fread(pszXML, 1, nContentLength, stdin) < nContentLength )
            WCTSEmitServiceException( "POST body is short." );

        pszXML[nContentLength] = '\0';
    }

    else
    {
        int nXMLMax, nXMLLen=0;

        nXMLMax = 100;
        pszXML = (char *) CPLMalloc(nXMLMax);
        
        while( !feof(stdin) )
        {
            pszXML[nXMLLen++] = fgetc(stdin);
            if( nXMLLen == nXMLMax )
            {
                nXMLMax = nXMLMax * 2;
                pszXML = (char *) CPLRealloc(pszXML, nXMLMax);
            }
        }

        pszXML[nXMLLen] = '\0';
    }

/* -------------------------------------------------------------------- */
/*      Convert into an XML document.                                   */
/* -------------------------------------------------------------------- */
    CPLErrorReset();

    CPLXMLNode *psTree = CPLParseXMLString( pszXML );
    CPLFree( pszXML );

    if( CPLGetLastErrorType() == CE_Failure )
        WCTSEmitServiceException( CPLGetLastErrorMsg() );

    return psTree;
}

/************************************************************************/
/*                        WCTSGetCapabilities()                         */
/*                                                                      */
/*      For now we just return a fixed capabilities document from       */
/*      the file system.  No real need to dynamically generate          */
/*      this except possibly to insert the coordinate system list       */
/*      based on scanning pcs.csv and gcs.csv.                          */
/************************************************************************/

void WCTSGetCapabilities( CPLXMLNode *psOperation )

{
/* -------------------------------------------------------------------- */
/*      Verify the service.                                             */
/* -------------------------------------------------------------------- */
    if( !EQUAL(CPLGetXMLValue(psOperation,"service","WCTS"),"WCTS") )
    {
        WCTSEmitServiceException( 
            CPLSPrintf( "Attempt to GetCapabilities for unsupported '%s'\n"
                        "service.  Only WCTS supported.",
                        CPLGetXMLValue(psOperation,"service","WCTS") ) );
    }

/* -------------------------------------------------------------------- */
/*      Search for our capabilities document.                           */
/* -------------------------------------------------------------------- */
    const char *pszCapFilename;
    FILE *fp;

    pszCapFilename = CPLFindFile( "etc", "wcts_capabilities.xml.0.0.3" );

    if( pszCapFilename == NULL 
        || (fp = VSIFOpen( pszCapFilename, "rt")) == NULL )
    {
        WCTSEmitServiceException( "WCTS server misconfigured, unable to find capabilities document." );
    }

/* -------------------------------------------------------------------- */
/*      Emit the document.                                              */
/* -------------------------------------------------------------------- */
    int nLen;
    char *pszDoc;

    VSIFSeek( fp, 0, SEEK_END );
    nLen = VSIFTell( fp );
    VSIFSeek( fp, 0, SEEK_SET );
    
    pszDoc = (char *) CPLMalloc(nLen);
    VSIFRead( pszDoc, 1, nLen, fp );
    VSIFClose( fp );

    printf( "Content-type: test/xml%c%c", 10, 10 );

    VSIFWrite( pszDoc, 1, nLen, stdout );
    fflush( stdout );

    exit( 0 );
}

/************************************************************************/
/*                WCTSImportCoordinateReferenceSystem()                 */
/*                                                                      */
/*      This is a place holder. Eventually this will use                */
/*      OGRSpatialReference.importFromXML() when that has been          */
/*      updated to the GML 3.0 CRS formats.                             */
/************************************************************************/

OGRSpatialReference *
WCTSImportCoordinateReferenceSystem( CPLXMLNode *psXMLCRS )

{
    int nEPSGCode;

/* -------------------------------------------------------------------- */
/*      Get the EPSG code, and verify that it is in the EPSG            */
/*      codeSpace.                                                      */
/* -------------------------------------------------------------------- */
    if( !EQUAL(CPLGetXMLValue( psXMLCRS, "Identifier.codeSpace", "" ), "EPSG"))
    {
        WCTSEmitServiceException( "Failed to decode CoordinateReferenceSystem with missing,\n"
                                  "or non-EPSG Identifier.codeSpace" );
    }	

    nEPSGCode = atoi(CPLGetXMLValue( psXMLCRS, "Identifier.code", "0" ));

    if( nEPSGCode == 0 )
    {
        WCTSEmitServiceException( "Failed to decode CoordinateReferenceSystem with missing,\n"
                                  "or zero Identifier.code" );
    }								

/* -------------------------------------------------------------------- */
/*      Translate into EPSG format.                                     */
/* -------------------------------------------------------------------- */
    OGRSpatialReference oSRS;

    CPLErrorReset();
    if( oSRS.importFromEPSG( nEPSGCode ) != OGRERR_NONE )
    {
        if( strlen(CPLGetLastErrorMsg()) > 0 )
            WCTSEmitServiceException( CPLGetLastErrorMsg() );
        else
            WCTSEmitServiceException( 
                CPLSPrintf( "OGRSpatialReference::importFromEPSG(%d) failed.  Is this a defined EPSG code?", 
                            nEPSGCode ) );
    }

/* -------------------------------------------------------------------- */
/*      Return SRS.                                                     */
/* -------------------------------------------------------------------- */
    return oSRS.Clone();
}

/************************************************************************/
/*                        WCTSIsTransformable()                         */
/************************************************************************/

void WCTSIsTransformable( CPLXMLNode *psOperation )

{
    OGRSpatialReference *poSrcCRS, *poDstCRS;
    CPLXMLNode *psSrcXMLCRS, *psDstXMLCRS;

/* -------------------------------------------------------------------- */
/*      Translate the source CRS.                                       */
/* -------------------------------------------------------------------- */
    psSrcXMLCRS = CPLGetXMLNode( psOperation, 
                                 "SourceCRS.CoordinateReferenceSystem" );

    if( psSrcXMLCRS == NULL )
        WCTSEmitServiceException( "Unable to identify SourceCRS.CoordinateReferenceSystem" );

    poSrcCRS = WCTSImportCoordinateReferenceSystem( psSrcXMLCRS );

/* -------------------------------------------------------------------- */
/*      Translate the destination CRS.                                  */
/* -------------------------------------------------------------------- */
    psDstXMLCRS = CPLGetXMLNode( psOperation, 
                                 "DestinationCRS.CoordinateReferenceSystem" );

    if( psDstXMLCRS == NULL )
        WCTSEmitServiceException( "Unable to identify DestinationCRS.CoordinateReferenceSystem" );

    poDstCRS = WCTSImportCoordinateReferenceSystem( psDstXMLCRS );

/* -------------------------------------------------------------------- */
/*      Create a transformation object between the coordinate           */
/*      systems as an added step of verification that they are          */
/*      supported.                                                      */
/* -------------------------------------------------------------------- */
    OGRCoordinateTransformation *poCT;
    const char *pszResult;

    poCT = OGRCreateCoordinateTransformation( poSrcCRS, poDstCRS );
    if( poCT == NULL )
        pszResult = "false";
    else
    {
        delete poCT;
        pszResult = "true";
    }

    delete poSrcCRS;
    delete poDstCRS;

/* -------------------------------------------------------------------- */
/*      Return the answer.                                              */
/* -------------------------------------------------------------------- */
    printf( "Content-type: text/xml%c%c", 10, 10 );

    printf( "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" );
    printf( "<TransformableResponse xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xsi:noNamespaceSchemaLocation=\"http://www.deegree.org/xml/schemas/wcts/transformableResponse.xsd\" transformable=\"%s\"/>\n", 
            pszResult );

    exit( 0 );
}

/************************************************************************/
/*                           WCTSTransform()                            */
/************************************************************************/

void WCTSTransform( CPLXMLNode *psOperation )

{
    
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main( int nArgc, char ** papszArgv )

{
    RegisterOGRGML();
    CPLPushFinderLocation(".");

/* -------------------------------------------------------------------- */
/*      Collect the request as a parsed XML document.                   */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psRequest;

    psRequest = WCTSCollectRequest();

/* -------------------------------------------------------------------- */
/*      Scan for known operation nodes.                                 */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psOperation;

    for( psOperation = psRequest; 
         psOperation != NULL; 
         psOperation = psOperation->psNext )
    {
        if( psOperation->eType == CXT_Element
            && EQUAL(psOperation->pszValue,"GetCapabilities") )
        {
            WCTSGetCapabilities( psOperation );
            assert( FALSE );
        }
        else if( psOperation->eType == CXT_Element
            && EQUAL(psOperation->pszValue,"Transformable") )
        {
            WCTSIsTransformable( psOperation );
            assert( FALSE );
        }
        else if( psOperation->eType == CXT_Element
            && EQUAL(psOperation->pszValue,"Transform") )
        {
            WCTSTransform( psOperation );
            assert( FALSE );
        }
        else if( psOperation->eType == CXT_Element
            && EQUAL(psOperation->pszValue,"DescribeTransformation") )
        {
            WCTSEmitServiceException( "This server does not support the DescribeTransformation operation." );
        }
    }

    WCTSEmitServiceException( "No recognisable supported request found." );
    exit( 1 );
}

