// ---- OpenGL -----
#include <GL/glut.h>
// ---- OpenCV -----
#include <cv.h>
#include <highgui.h>
// -- libfreenect --
#include "libfreenect.h"
#include "libfreenect_sync.h"
#include "libfreenect_cv.h"
// --- C++ ---
#include <stdio.h>
#include <fstream>
#include <vector>
#include <math.h>

using namespace cv;

// Shorten the name for easier typing
typedef pair< Vec3f, Vec3f > match;
typedef vector< match > matchList;
matchList correspondences;
match centroids( Vec3f(0,0,0), Vec3f(0,0,0) );

//lighting stuff
const GLfloat light_ambient[]  = { 0.4f, 0.4f, 0.04, 1.0f };
const GLfloat light_diffuse[]  = { 1.0f, 1.0f, 1.0f, 1.0f };
const GLfloat light_specular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
const GLfloat light_position[] = { 2.0f, 5.0f, 5.0f, 0.0f };

const GLfloat mat_ambient[]    = { 0.2f, 0.2f, 1.0f, 1.0f };
const GLfloat mat_diffuse[]    = { 0.2f, 0.8f, 1.0f, 1.0f };
const GLfloat mat_specular[]   = { 1.0f, 1.0f, 1.0f, 1.0f };
const GLfloat high_shininess[] = { 100.0f };

const GLfloat mat_ambient_back[]    = { 0.5f, 0.2f, 0.2f, 1.0f };
const GLfloat mat_diffuse_back[]    = { 1.0f, 0.2f, 0.2f, 1.0f };


// Variables for Calculations and Loops
const int NUM_CAMS = 2;
GLuint rgbTexGL;
int window;
float angle_between_cams = 0.f;
int mx=-1,my=-1;        // Prevous mouse coordinates
int rotangles[2] = {0}; // Panning angles
float zoom = 1;         // zoom factor
int color = 1;          // Use the RGB texture or just draw it as color

// Window Size and Position
const int window_width = 640, window_height = 480;
int window_xpos = 1000, window_ypos = 100;

// OpenGl Initialization Routines
void setupGL( int argc, char** argv );
void define_lights_and_materials();

// Callback Functions
void cbRender();
void cbTimer( int ms );
void cbReSizeGLScene( int Width, int Height);
void cbKeyPressed( unsigned char key, int x, int y);
void cbMouseMoved( int x, int y);
void cbMousePress( int button, int state, int x, int y);

// Called insde the display function
void kernel();
void loadVertexMatrix();
void loadRGBMatrix();
void noKinectQuit();
void draw_axes();
void draw_line(Vec3b v1, Vec3b v2);

void loadBuffers( int cameraIndx, 
        unsigned int indices[window_height][window_width], 
        short xyz[window_height][window_width][3], 
        unsigned char rgb[window_height][window_width][3] );

// Computer Vision Functions
Mat joinFrames( const Mat& img1, const Mat& img2 );
matchList findFeatures( const Mat& img1, const Mat& img2 );
match calcCentroids( const matchList& corrs, const Mat& img1, const Mat& img2 );
Mat calcRotation( match centr, matchList corrs );
float getDepth( int cam, int x, int y );
void printMat( const Mat& A );

// Store the matrices from all cameras here
vector<Mat> rgbCV;
vector<Mat> depthCV;

// Rotation Matrix
Mat rot( 3, 3, CV_32F );

// To know when registered
bool REGISTERED = false;



int main( int argc, char** argv ) {

#if 0
    printf("sizeof(int) = %d\n", sizeof(int));
    printf("sizeof(short) = %d\n", sizeof(short));
    printf("sizeof(unsigned char) = %d\n", sizeof(unsigned char));
    printf("sizeof(char) = %d\n", sizeof(char));
#endif

    // load the first frames 
    for( int cam = 0; cam < NUM_CAMS; cam++ ) {
        rgbCV.push_back( freenect_sync_get_rgb_cv(cam) );
        depthCV.push_back( freenect_sync_get_depth_cv(cam) );
    }

    setupGL( argc, argv );
    void define_lights_and_materials();
    glutMainLoop();

    return 0;
}

void setupGL( int argc, char** argv ) {

    // Initialize Display Mode
    glutInit( &argc, argv );
    glutInitDisplayMode( GLUT_RGB | GLUT_DEPTH );
    glutInitDisplayMode( GLUT_RGBA | GLUT_DOUBLE | GLUT_ALPHA | GLUT_DEPTH );

    // Initialize Window
    glutInitWindowSize( window_width, window_height );
    glutInitWindowPosition( window_xpos, window_ypos );
    window = glutCreateWindow("Kinect Registration");

    // Textures and Color
    glClearColor( 0.0f, 0.0f, 0.0f, 0.0f);
    glEnable( GL_DEPTH_TEST);
    glGenTextures( 1, &rgbTexGL);
    glBindTexture( GL_TEXTURE_2D, rgbTexGL);
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Setup The Callbacks
    glutDisplayFunc( &cbRender );
    glutIdleFunc( &cbRender );
    glutReshapeFunc( &cbReSizeGLScene);
    cbReSizeGLScene( window_width, window_height);
    glutKeyboardFunc( &cbKeyPressed);
    glutMotionFunc( &cbMouseMoved);
    glutMouseFunc( &cbMousePress);

    // The time passed in here needs to be the same as waitKey() for OpenCV
    glutTimerFunc( 10, cbTimer, 10);

    glScalef( .5, .5, .5 );
    glPushMatrix();
    glClear( GL_COLOR_BUFFER_BIT );

}

void cbKeyPressed( unsigned char key, int x, int y ) {

    // Press esc to exit
    if ( key == 27 ) {
        freenect_sync_stop();
        glutDestroyWindow( window );
        exit( 0 );
    }
    if( key == 'u' ) {
        REGISTERED = false;
    }
    if( key == 't' ) {
        float r[] = {0,0,1,0,1,0,-1,0,0};
        rot = Mat( 3, 3, CV_32F, r );
        REGISTERED = true;
    }
    if( key == 'f' ) {
        correspondences = findFeatures( rgbCV[0], rgbCV[1] );
        centroids = calcCentroids( correspondences, rgbCV[0], rgbCV[1] );
        rot = calcRotation( centroids, correspondences );
        REGISTERED = true;
    }
    if ( key == 'w' )
        zoom *= 1.1f;
    if ( key == 's' )
        zoom /= 1.1f;
    if ( key == 'c' )
        color = !color;
    if ( key == 't' ) {}
}

void cbMouseMoved( int x, int y) {

    if ( mx>=0 && my>=0) {
        rotangles[0] += y-my;
        rotangles[1] += x-mx;
    }

    mx = x;
    my = y;

}

void cbMousePress( int button, int state, int x, int y) {

    if ( button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        mx = x;
        my = y;
    }

    if ( button == GLUT_LEFT_BUTTON && state == GLUT_UP) {
        mx = -1;
        my = -1;
    }
}

void cbRender() {

    unsigned int indices[NUM_CAMS*window_height][window_width];
    short xyz[NUM_CAMS*window_height][window_width][3];
    unsigned char rgb[NUM_CAMS*window_height][window_width][3];

    // Flush the OpenCV Mat's from last frame
    rgbCV.clear();
    depthCV.clear();

    // Todo: finish processing BEFORE clearing or we might 
    // process too long and see blank screen
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
    glLoadIdentity();

    //------------------------------

    for( int cam = 0; cam < NUM_CAMS; cam++ ) {

        loadBuffers( cam, &indices[cam*window_height], &xyz[cam*window_height], &rgb[cam*window_height] ); 

    }
    glPushMatrix();
    glScalef( zoom,zoom,1 );
    glTranslatef( 0,0,-3.5 );
    glRotatef( rotangles[0], 1,0,0 );
    glRotatef( rotangles[1], 0,1,0 );
    glTranslatef( 0,0,1.5 );
    draw_axis();

    if ( REGISTERED ) {
        //-------------------------------------------
        glMatrixMode( GL_MODELVIEW );
        glPushMatrix();
        glColor3f(0,1,0);
        loadVertexMatrix();
        glTranslatef( centroids.first[0],centroids.first[1],centroids.first[2] );
        glBegin( GL_LINE_LOOP );/// don't workglPointSize( 0.0 );
        GLUquadricObj *quadric;
        quadric = gluNewQuadric();

        gluQuadricDrawStyle(quadric, GLU_FILL );
        gluSphere( quadric , 10 , 16 , 18 );


        gluDeleteQuadric(quadric); 
        glEndList();

        glEnd();

        glPopMatrix();

        // ---------------------------------

        glPushMatrix();
        glColor3f(1,0,0);
        loadVertexMatrix();
        glTranslatef( centroids.second[0],centroids.second[1],centroids.second[2] );
        glBegin( GL_LINE_LOOP );/// don't workglPointSize( 0.0 );
        //GLUquadricObj *quadric;
        quadric = gluNewQuadric();

        gluQuadricDrawStyle(quadric, GLU_FILL );
        gluSphere( quadric , 10 , 16 , 18 );


        gluDeleteQuadric(quadric); 
        glEndList();

        glEnd();

        glPopMatrix();

        //---------------------------------------------
    }
#if 0
    //TODO: 
    if( true ) {	
        if( cam == 0 ) 
            glTranslatef( -centroids.first[0]/640.0f, -centroids.first[1]/480.0f, -centroids.first[2] );
        else
            glTranslatef( -centroids.second[0]/640.0f, -centroids.second[1]/480.0f, -centroids.second[2] );
    }
#endif 
    //------------------------------
    loadVertexMatrix();
    //------------------------------

    // Set the projection from the XYZ to the texture image
    glMatrixMode( GL_TEXTURE) ;
    glLoadIdentity();
    //        glScalef( 1/640.0f,1/480.0f,1 );
    glScalef( 1./(window_width),1./(window_height*NUM_CAMS),1 );
    loadRGBMatrix();

    // TODO: Load a unique projection per camera to calibrate together

    //------------------------------
    loadVertexMatrix();
    //------------------------------

    glMatrixMode( GL_MODELVIEW );
    glPointSize( 1 );

#if 0
    // ---------------
    glEnableClientState( GL_VERTEX_ARRAY );
    glVertexPointer( 3, GL_SHORT, 0, xyz );
    glEnableClientState( GL_TEXTURE_COORD_ARRAY ); 
    glTexCoordPointer( 3, GL_SHORT, 0, xyz );

    // ---------------
    glEnableClientState( GL_VERTEX_ARRAY );
    if ( color ) 
        glEnable( GL_TEXTURE_2D );
    glBindTexture( GL_TEXTURE_2D, rgbTexGL );
    glTexImage2D( GL_TEXTURE_2D, 0, 3, window_width, NUM_CAMS*window_height, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb );

    // ---------------
    glPointSize( 2.0f );
    glDrawElements( GL_POINTS, NUM_CAMS*window_width*window_height, GL_UNSIGNED_INT, indices );
#endif

#if 1
    //printf("color buffer\n");
    //glColor3f(1,0,0);
    glVertexPointer( 3, GL_SHORT, 0, xyz );

    glColorPointer( 3, GL_UNSIGNED_BYTE, 0, rgb );

    //printf("enable client state\n");

    glEnableClientState(GL_VERTEX_ARRAY);

    glEnableClientState(GL_COLOR_ARRAY);

    glDrawArrays(GL_POINTS, 0, 2*window_width*window_height);

#endif
    // ---------------

    glPopMatrix();
    // ---------------------------------



    glDisable( GL_TEXTURE_2D );

    kernel();
    glutSwapBuffers();
}

void kernel() {

    if( correspondences.empty() ) {

        Mat tmp = rgbCV[0].clone();
        for( int cam = 0; cam < NUM_CAMS; cam++ ) {
            cvtColor( rgbCV[cam], tmp, CV_RGB2BGR );
            rgbCV[cam] = tmp.clone();
        }

        Mat rgb = joinFrames( rgbCV[0], rgbCV[1] );
        imshow( "Camera 0 | Camera 1", rgb );

        // Time here needs to be the same as cbTimer
        // returns -1 if no key pressed
        char key = waitKey( 10 );

        // If someone presses a button while a cv window 
        // is in the foreground we want the behavior to
        // be the same as for the OpenGL window, so call 
        // OpenGL's cbKeyPressed callback function
        if( key != -1 ) 
            cbKeyPressed( key, 0, 0 );
    }
}

// This ensures that OpenGL and OpenCV play nicely together
void cbTimer( int ms ) {

    glutTimerFunc( ms, cbTimer, ms );
    glutPostRedisplay();

}

void cbReSizeGLScene( int Width, int Height) {

    glViewport( 0,0,Width,Height );
    glMatrixMode( GL_PROJECTION );
    glLoadIdentity();
    gluPerspective( 60, 4/3., 0.3, 200 );
    glMatrixMode( GL_MODELVIEW );
}


void noKinectQuit() {
    printf( "Error: Kinect not connected?\n" );
    exit( 1 );
}

void loadBuffers( int cameraIndx, 
        unsigned int indices[window_height][window_width], 
        short xyz[window_height][window_width][3], 
        unsigned char rgb[window_height][window_width][3] ) {

    // Load the new Mats
#if 0
    Mat d = freenect_sync_get_rgb_cv(cameraIndx);
    short d1 = d.at<short>(0,0);
    unsigned char d2 = d.at<unsigned char>(0,0);
    char d3 = d.at<char>(0,0);
    printf("\n-------------------SHORT-------------\n");
    printf("\n SIZEOF( depth.at<short>(0,0) ) = %d\n", sizeof(d1));
    printf("depth.at<short>(0,0) = %d\n", d1);
    printf("\n--------------UNSIGNED CHAR-------------\n");
    printf("\n SIZEOF( depth.at<unsigned char>(0,0) ) = %d\n", sizeof(d2));
    printf("depth.at<unsigned char>(0,0) = %d\n", d2);
    printf("\n------------------CHAR-------------\n");
    printf("\n SIZEOF( depth.at<char>(0,0) ) = %d\n", sizeof(d3));
    printf("depth.at<char>(0,0) = %d\n", d3);
#endif

    rgbCV.push_back( freenect_sync_get_rgb_cv(cameraIndx) );
    depthCV.push_back( freenect_sync_get_depth_cv(cameraIndx) );

    int cntrx, cntry, cntrz;

    if( REGISTERED && cameraIndx == 0 ) {
        // Translation
        float x = centroids.second[0]-centroids.first[0]; 
        float y = centroids.second[1]-centroids.first[1]; 
        float z = centroids.second[2]-centroids.first[2]; 

        for ( int i = 0; i < window_height; i++ ) {
            for ( int j = 0; j < window_width; j++ ) {

                float data[] = {j, i, (float)depthCV[cameraIndx].at<short>( i, j )};
                Mat pt( 3, 1, CV_32F, data );

                pt = rot*pt;

                xyz[i][j][0] = (short)(pt.at<float>(0,0)+x);
                xyz[i][j][1] = (short)(pt.at<float>(1,0)+y);               
                xyz[i][j][2] = (short)(pt.at<float>(2,0)+z); 
                indices[i][j] = (cameraIndx * window_height + i)*window_width + j; 

                Vec3b color = rgbCV[cameraIndx].at<Vec3b>(i,j); 
                rgb[i][j][0] = color[0];
                rgb[i][j][1] = color[1];
                rgb[i][j][2] = color[2];
            }
        }
    }
    else {

        for ( int i = 0; i < window_height; i++ ) {
            for ( int j = 0; j < window_width; j++ ) {
                xyz[i][j][0] = j;
                xyz[i][j][1] = i;
                xyz[i][j][2] = depthCV[cameraIndx].at<short>( i, j );
                indices[i][j] = (cameraIndx * window_height + i)*window_width + j; 

                Vec3b color = rgbCV[cameraIndx].at<Vec3b>(i,j); 
                rgb[i][j][0] = color[0];
                rgb[i][j][1] = color[1];
                rgb[i][j][2] = color[2];
            }
        }
    }
}

// Do the projection from u,v,depth to X,Y,Z directly in an opengl matrix
// These numbers come from a combination of the ros kinect_node wiki, and
// nicolas burrus' posts.
void loadVertexMatrix() {

    float fx = 594.21f;
    float fy = 591.04f;
    float a = -0.0030711f;
    float b = 3.3309495f;
    float cx = 339.5f;
    float cy = 242.7f;
    GLfloat mat[16] = {
        1/fx,     0,  0, 0,
        0,    -1/fy,  0, 0,
        0,       0,  0, a,
        -cx/fx, cy/fy, -1, b
    };
    glMultMatrixf( mat);
}

// This matrix comes from a combination of nicolas burrus's calibration post
// and some python code I haven't documented yet.
void loadRGBMatrix() {

    float mat[16] = {
        5.34866271e+02,   3.89654806e+00,   0.00000000e+00,   1.74704200e-02,
        -4.70724694e+00,  -5.28843603e+02,   0.00000000e+00,  -1.22753400e-02,
        -3.19670762e+02,  -2.60999685e+02,   0.00000000e+00,  -9.99772000e-01,
        -6.98445586e+00,   3.31139785e+00,   0.00000000e+00,   1.09167360e-02
    };
    glMultMatrixf( mat);
}

Mat joinFrames( const Mat& img1, const Mat& img2 ) {

    Mat rslt = Mat::zeros( img1.rows, img1.cols*2, img1.type());
    for( int i = 0; i < img1.rows; i++ )
        for( int j = 0; j < img1.cols; j++ ) {
            Vec3b p = img1.at<Vec3b>( i,j);
            Vec3b q = img2.at<Vec3b>(i,j);
            rslt.at<Vec3b>(i,j) = p;
            rslt.at<Vec3b>(i,j+window_width) = q;
        }

    return rslt;
}

matchList findFeatures( const Mat& img1, const Mat& img2 ) {

    // Store all of the SIFT features and their respective correspondences here
    vector<KeyPoint> keypoints1, keypoints2;
    Mat descriptors1, descriptors2;

    printf( "threshold = %f\n", cv::SIFT::DetectorParams::GET_DEFAULT_THRESHOLD());
    printf( "Edgethreshold = %f\n", cv::SIFT::DetectorParams::GET_DEFAULT_EDGE_THRESHOLD());

    // Control the first parameter to increase or decrease the number of features found.
    // Lowering the threshold, increases the features. Raising it has the inverse effect.
    SiftFeatureDetector sift( .06, 10);

    printf( "\nFinding Features....\n\n");
    // Detect sift features and store them in the keypoints
    sift.detect( img1, keypoints1 );
    sift.detect( img2, keypoints2 );

    printf( "Features Found!\n\n");
    printf( "Each Potential match is drawn in Red. \n\n1) If it's an accurate correspondence "  
            "\n\n    press 'y' for yes \n\n    press any other key for no \n\n2) The selected corresondences "
            "will be drawn in green on the screen. \n3) After each selection, "  
            "more 'bad' correspondences will be filtered out from the remaining matches.\n");

    // Extract the descriptors from each image for comparison and matching
    SiftDescriptorExtractor extractor;
    extractor.compute( img1, keypoints1, descriptors1 );
    extractor.compute( img2, keypoints2, descriptors2 );

    // Do some matching!
    BruteForceMatcher< L2<float> > matcher;
    vector<DMatch> matches;
    matcher.match( descriptors1, descriptors2, matches );

    // Draw the matches on to the img_matches matrix
    Mat img_matches;
    drawMatches( img1, keypoints1, img2, keypoints2, matches, img_matches);

    // Put both images side by side in one image
    Mat rslt = joinFrames( img1, img2 );

    // -----------------Examine each coorespondence one at a time--------------

    // Store the cumulative slopes of correspondences here
    double totSlope;
    // This is where we'll store the correspondences for future 
    matchList corrs;

    int i;
    for( i=0; i < (int)matches.size(); i++ ) {
        Mat tmp = rslt.clone();
        // These are the actual points in the matrices
        Point2f pt1 = keypoints1[matches[i].queryIdx].pt;
        Point2f pt2 = keypoints2[matches[i].trainIdx].pt;
        // These will be the points transformed to the joined image coordinate system
        // to use in slope calculations and comparisons
        Point2f transpt1 = Point2f( pt1.x,window_height-pt1.y);
        Point2f transpt2 = Point2f( pt2.x+window_width,window_height-pt2.y);
        // Draw circles around the correspondences
        circle( tmp, pt1, 3, Scalar( 0,0,255), 1, CV_AA );
        circle( tmp, pt2+Point2f(window_width,0), 3, Scalar(0,0,255), 1, CV_AA );
        line( tmp, pt1, pt2+Point2f( (float)window_width,0.0), Scalar(0,0,255), 1, CV_AA );
        // Show this match and ask the user if it's an accurate correspondence
        imshow( "Matching Correspondences", tmp);
        char c = waitKey( 0);
        if( c == 'y' || c == 'Y' ) {
            // Store the correspondences in a vector of pairs, where the members of each pair are points
            corrs.push_back( match( Vec3f(pt1.x,pt1.y,1), Vec3f(pt2.x,pt2.y,1) ) );
            // Calculate the slope of the line between these, to filter out bad future matches
            float slp = (transpt2.y-transpt1.y)/(transpt2.x-transpt1.x);
            totSlope = slp; 
            printf("-------------------MACTH ACCEPTED------------------\n");
            printf("(x1, y1) (x2, y2) = (%f,%f) (%f,%f)\n", transpt1.x, transpt1.y, transpt2.x, transpt2.y);
            printf("pt = %d, slope = %f\n",i,slp);
            break;
        }
    }
    // Continue going through the matches to find more accurate correspondences
    for( i=i+1; i < (int)matches.size(); i++ ) {
        bool broken = false;
        Mat tmp = rslt.clone();
        // These are the actual points in the matrices
        Point2f pt1 = keypoints1[matches[i].queryIdx].pt;
        Point2f pt2 = keypoints2[matches[i].trainIdx].pt;
        // These will be the points transformed to the joined image coordinate system
        // to use in slope calculations and comparisons
        Point2f transpt1 = Point2f(pt1.x,window_height-pt1.y);
        Point2f transpt2 = Point2f(pt2.x+window_width,window_height-pt2.y);
        float slp = (transpt2.y-transpt1.y)/(transpt2.x-transpt1.x);
        printf("\n-----------CORRESPONDENCE CANDIDATE--------\n"); 
        printf("(x1, y1) (x2, y2) = (%f,%f) (%f,%f)\n", transpt1.x, transpt1.y, transpt2.x, transpt2.y);
        printf( "match # = %d, slope = %f, abs( slp - average )  = %f\n", i, slp, abs(slp - totSlope/corrs.size()) );
        printf("Avgerage Slope = %f\n", totSlope/corrs.size());
        printf("Correspondences Found = %d\n", (int)corrs.size());

        // ---------------------------Filter Out Points----------------------

        // If slope is not close to average don't even look at it
        if( abs( slp - totSlope/corrs.size() ) > .05 ) {
            printf("-----------------INVALID SLOPE---------------\n");
            printf( "slope = %f, abs( slp - average )  = %f\n", slp, abs(slp - totSlope/corrs.size()) );
            continue;
        }

        // If this point is one of the current correspondences, get rid of it
        // because one point can't have two distinct correspondences
        for( int j = 0; j < (int)corrs.size(); j++ ) {

            // Draw current correspondences in green
            Point2f firstpt2D = Point2f(corrs[j].first[0], corrs[j].first[1]);
            Point2f secondpt2D = Point2f(corrs[j].second[0], corrs[j].second[1]);
            circle( tmp, firstpt2D, 3, Scalar(0,255,0), 1, CV_AA );
            circle( tmp, secondpt2D+Point2f(window_width,0), 3, Scalar(0,255,0), 1, CV_AA );
            line( tmp, firstpt2D, secondpt2D+Point2f(window_width,0), Scalar(0,255,0), 1, CV_AA );
            if( pt1 == firstpt2D || pt2 == secondpt2D ) {
                broken = true;
                break;
            }
        }
        if( broken ) {
            broken = false;
            continue;
        }

        // -------------------------------------------------------------------

        // Draw each match in question in Red so the user can see it and decide if it's accurate
        circle( tmp, pt1, 3, Scalar(0,0,255), 1, CV_AA );
        circle( tmp, pt2+Point2f(window_width,0), 3, Scalar(0,0,255), 1, CV_AA );
        line( tmp, pt1, pt2+Point2f((float)window_width,0.0), Scalar(0,0,255), 1, CV_AA );
        imshow("Matching Correspondences", tmp);
        char c = waitKey(0);
        if( c == 'y' || c == 'Y' ) {
            // Store the correspondences in a vector of pairs, where the members of each pair are points
            corrs.push_back( match( Vec3f(pt1.x,pt1.y,1), Vec3f(pt2.x,pt2.y,1) ) );
            totSlope += slp;
            printf("-------------------MACTH ACCEPTED------------------\n");
            printf("(x1, y1) (x2, y2) = (%f,%f) (%f,%f)\n", transpt1.x, transpt1.y, transpt2.x, transpt2.y);
            printf("pt = %d, slope = %f\n",i,slp);
        }
        else if( c == 'q' ) {
            break;
        }
        else printf("-------------------MATCH DENIED---------------\n");
    }

    printf("\n\nCorrespondences have been found!\n\n");

    // Show all of the matches to compare to those the user decided to be accurate correspondences
    imshow("SIFT matches", img_matches);

    return corrs;

}

match calcCentroids( const matchList& corrs, const Mat& img1, const Mat& img2 ) {

    printf( "\n\nCalculating the centroids for each cloud of correspondences\n" );

    Vec3f centr1(0,0,0), centr2(0,0,0);
    float z1, z2;

    for( int i = 0; i < (int)corrs.size(); i ++ ) {

        // First image 
        centr1[0] += corrs[i].first[0]; 
        centr1[1] += corrs[i].first[1]; 
        //z1 = (float)depthCV[0].at<short>( (int)centr1[0], (int)centr1[1] );
        z1 = getDepth( 0, (int)corrs[i].first[0], (int)corrs[i].first[1] );
        centr1[2] += z1; 

        // Second image
        centr2[0] += corrs[i].second[0];
        centr2[1] += corrs[i].second[1];
        //z1 = (float)depthCV[1].at<short>( (int)centr2[0], (int)centr2[1] );
        z2 = getDepth( 1, (int)corrs[i].second[0], (int)corrs[i].second[1] );
        centr2[2] += z2; 

        printf( "First %d: (%f,%f,%f)\n", i, corrs[i].first[0], corrs[i].first[1], z1 );
        printf( "Second %d: (%f,%f,%f)\n", i, corrs[i].second[0], corrs[i].second[1], z2 );
    }

    // first
    centr1[0] /= corrs.size();
    centr1[1] /= corrs.size();
    centr1[2] /= corrs.size();
    // second
    centr2[0] /= corrs.size();
    centr2[1] /= corrs.size();
    centr2[2] /= corrs.size();

    match centroids( centr1, centr2 );
#if 0
    printf( "\nCorrespondence points in Image 1\n" );
    for( int i = 0; i < (int)corrs.size(); i++ ) 
        printf( "%d: (%f,%f,%f)\n", i, corrs[i].first[0], corrs[i].first[1], z1 );
    printf( "\nCorrespondence points in Image 2\n" );
    for( int i = 0; i < (int)corrs.size(); i++ ) 
        printf( "%d: (%f,%f,%f)\n", i, corrs[i].second[0], corrs[i].second[1], z2 );
#endif

    // Put both images side by side in one image
    Mat rslt = joinFrames( img1, img2 );

    // Draw green circles around the correspondences in each image
    for( int i = 0; i < (int)corrs.size(); i++ ) {
        circle( rslt, Point2f( corrs[i].first[0], corrs[i].first[1] ), 3, Scalar(0,255,0), 1, CV_AA );
        circle( rslt, Point2f( corrs[i].second[0], corrs[i].second[1] )
                +Point2f(window_width,0), 3, Scalar(0,255,0), 1, CV_AA );
    }

    // Draw red circles around the centroids
    circle( rslt, Point2f(centr1[0],centr1[1]), 3, Scalar(0,0,255), 1, CV_AA );
    circle( rslt, Point2f(centr2[0],centr2[1])+Point2f(window_width,0), 3, Scalar(0,0,255), 1, CV_AA );

    printf("\nCentroid 1 (x,y,z) = (%f,%f,%f)\n", centr1[0], centr1[1], centr1[2]);
    printf("Centroid 2 (x,y,z) = (%f,%f,%f)\n\n", centr2[0], centr2[1], centr2[2]);
    imshow("Centroids", rslt);

    waitKey(0);

    return centroids;
} 

Mat calcRotation( match centr, matchList corrs ) {
#if 1

    Mat P = Mat::ones( 3, corrs.size(), CV_32F );
    Mat Q = Mat::ones( 3, corrs.size(), CV_32F );

    // This is retarded... Need to restructure
    for( int pt = 0; pt < corrs.size(); pt++ ) {
        // first point cloud
        float x = corrs[pt].first[0]; 
        float y = corrs[pt].first[1];
        //float d = (float)depthCV[0].at<short>( (int)x, (int)y );
        float d = getDepth( 0, (int)x, (int)y );
        P.at<float>(0,pt) = x-centr.first[0]; 
        P.at<float>(1,pt) = y-centr.first[1]; 
        P.at<float>(2,pt) = d; 
        // second point cloud
        float x2 = corrs[pt].second[0]; 
        float y2 = corrs[pt].second[1];
        //float d2 = (float)depthCV[1].at<short>( (int)x2, (int)y2 );
        float d2 = getDepth( 1, (int)x2, (int)y2 );
        Q.at<float>(0,pt) = x2-centr.second[0]; 
        Q.at<float>(1,pt) = y2-centr.second[1]; 
        Q.at<float>(2,pt) = d2; 
    }

    Mat Qt;
    transpose( Q, Qt );
    Mat PQt = P*Qt;

    //printf(" corrs[0].first[0] = %f\n", corrs[0].first[0] );
    //printf(" corrs[0].first[1] = %f\n", corrs[0].first[1] );
    //printf(" corrs[0].first[2] = %f\n", corrs[0].first[2] );
    //printf(" corrs[0].first[3] = %f\n", corrs[0].first[3] );

    printf(" P.rows = %d\n", P.rows );
    printf(" P.cols = %d\n\n", P.cols );

    printMat( P );	

    printf("\n Q.rows = %d\n", Q.rows );
    printf(" Q.cols = %d\n", Q.cols );

    printMat( Q );	

    printf(" PQt.rows = %d\n", PQt.rows );
    printf(" PQt.cols = %d\n", PQt.cols );

    SVD svd( PQt );

    printf(" svd.u.rows = %d\n", svd.u.rows );
    printf(" svd.u.cols = %d\n", svd.u.cols );

    printMat( svd.u );	

    printf(" svd.vt.rows = %d\n", svd.vt.rows );
    printf(" svd.vt.cols = %d\n", svd.vt.cols );

    printMat( svd.vt );	

    Mat R = svd.u*svd.vt;

    printf(" R.rows = %d\n", R.rows );
    printf(" R.cols = %d\n", R.cols );

    printMat( R );	


    glutPushWindow();
    cvDestroyWindow( "Camera 0 | Camera 1" );
    cvDestroyWindow( "Matching Correspondences" );
    cvDestroyWindow( "SIFT matches" );
    cvDestroyWindow( "Centroids" );
    return R;
#endif 
}

void printMat( const Mat& A ) {

    printf("\n| ");
    for(int i = 0; i < A.rows; i++) {
        if(i > 0 && i < A.rows) {
            printf("|\n");
            printf("| ");
        }
        for(int j = 0; j < A.cols; j++) {
            printf( "%f ", A.at<float>(i,j) );
        }
    }
    printf("|\n");
}

// Definitely need to come up with a better
// way to do this...
float getDepth( int cam, int x, int y ) {

    float d = (float)depthCV[cam].at<short>(x,y);
    printf("short, float = %d, %f\n",depthCV[cam].at<short>(x,y), d);

    if( d >= 2047 ) 
        d = (float)depthCV[cam].at<short>(x,y+1);

    if( d >= 2047 ) 
        d = (float)depthCV[cam].at<short>(x,y-1);

    if( d >= 2047 ) 
        d = (float)depthCV[cam].at<short>(x+1,y);

    if( d >= 2047 ) 
        d = (float)depthCV[cam].at<short>(x-1,y);

    if( d >= 2047 )
        d = (float)depthCV[cam].at<short>(x-1,y-1);

    if( d >= 2047 )
        d = (float)depthCV[cam].at<short>(x-1,y+1);

    if( d >= 2047 )
        d = (float)depthCV[cam].at<short>(x+1,y-1);

    if( d >= 2047 )
        d = (float)depthCV[cam].at<short>(x+1,y+1);

    return d;
}

void define_lights_and_materials()
{

    // Ambient: color in the absence of light
    glLightfv(GL_LIGHT0, GL_AMBIENT,  light_ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE,  light_diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular);
    glLightfv(GL_LIGHT0, GL_POSITION, light_position);

    glMaterialfv(GL_FRONT, GL_AMBIENT,   mat_ambient);
    glMaterialfv(GL_FRONT, GL_DIFFUSE,   mat_diffuse);
    glMaterialfv(GL_FRONT, GL_SPECULAR,  mat_specular);
    glMaterialfv(GL_FRONT, GL_SHININESS, high_shininess);

    glMaterialfv(GL_BACK, GL_AMBIENT,   mat_ambient_back);
    glMaterialfv(GL_BACK, GL_DIFFUSE,   mat_diffuse_back);
    glMaterialfv(GL_BACK, GL_SPECULAR,  mat_specular);
    glMaterialfv(GL_BACK, GL_SHININESS, high_shininess);

    // lighting only has effect if lighting is on
    // color of vertices have no effect without material 
    //   if phone is one (GL_SMOOTH)

    glEnable(GL_LIGHT0);
    //glEnable(GL_LIGHT_MODEL_TWO_SIDE);

    // I don't really undersand GL_COLOR_MATERIAL
    glEnable(GL_COLOR_MATERIAL);
    //glEnable(GL_LIGHTING);
    glEnable(GL_NORMALIZE); // makes sure normal remain unit length

    // Blending is only active when blending mode is ON
    // Page 129 in GLUT course
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void draw_line( Vec3b v1, Vec3b v2) {

    glBegin(GL_LINES);
    glVertex3f(v1[0], v1[1], v1[2]);
    glVertex3f(v2[0], v2[1], v2[2]);
    glEnd();

}




void draw_axes() {

    //X Axis

    glColor3f(1,0,0);    //red
    v1 = Vec3b(0,0,0);
    v2 = Vec(1,0,0);
    draw_line(v1, v2);

    //Y Axis

    glColor3f(0,1,0);    //green
    v1 = Vec3b(0,0,0);
    v2 = Vec3b(0,1,0);
    draw_line(v1, v2);

    //Z Axis

    glColor3f(0,0,1);    //blue
    v1 = Vec3b(0,0,0);
    v2 = Vec3b(0,0,1);
    draw_line(v1, v2);

}
