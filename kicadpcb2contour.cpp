/* Opens kicad PCB file
   Writes BW Image of tracks into map.png

 File Created
    map.png - BW image of tracks
    trace.png - BW contour of track expansion
    cpp_image.png - Colour track expansion
*/

//#define DEBUG

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include <iostream>
#include <fstream>
#include <list>
#include <algorithm>
#include <sstream>
#include <string>
#include <math.h>

using namespace cv;
using namespace std;

unsigned int pixels_per_mm = 30; // Tunable parameter resolves pixels per mm. Larger value slower calculation. Lower values coarser contours.

typedef cv::Point3_<uint8_t> Pixel;

bool color_pool[31*31*31];

#define RECT 1
#define ELLIPSE 2

#define K_RECT 1
#define K_ELLPSE 2
#define K_CIRCLE 3
#define K_ROUNDRECT 4
#define K_LINE 5
#define K_ARC 6

#define PADTYPE 0
#define LOCX 1
#define LOCY 2
#define SIZEX 3
#define SIZEY 4
#define HOLEX 5
#define HOLEY 6
#define RRATIO 7
#define ANGLE 8

#define LINEX1 0
#define LINEY1 1
#define LINEX2 2
#define LINEY2 3
#define LINEWIDTH 4

#define EDGETYPE 0
#define EDGESTARTX 1
#define EDGESTARTY 2
#define EDGEENDX 3
#define EDGEENDY 4
#define EDGEWIDTH 5
#define EDGEANGLE 6


Mat detected_edges;

using namespace std;
using namespace cv;

class PCBTRACKS
{
  private:
    float startx,starty,endx,endy;
    unsigned int istartx,istarty,iendx,iendy;

    static unsigned int count;


};

float linesegment[50000][5];
unsigned int iline[50000][5];
unsigned int linesegpos=0;

float ipadseg[50000][9];
unsigned int ipad[50000][9];
unsigned int ipadpos=0;

float edgecuts[10000][7];
unsigned int iedgecuts[10000][7];
unsigned int edgecutpos=0;


std::string gcode="";
unsigned int blockcounter=0;

float boardminx=9.0e+9,boardminy=9.0e+9,boardmaxx=-9.0e+9,boardmaxy=-9.0e+9;
unsigned image_height,image_width;

void rounded_rectangle( Mat& src, Point topLeft, Point bottomRight, const Scalar lineColor, const int thickness, const int lineType , const int cornerRadius)
{

}

bool is_number(const std::string& s)
{
    return !s.empty() && std::find_if(s.begin(),
        s.end(), [](unsigned char c) { return !std::isdigit(c); }) == s.end();
}

void get_two_vals(std::string line, float *x, float *y)
{
    size_t pos =0;
    std::string token;

    line.erase(remove(line.begin(), line.end(), ')'), line.end());

    int posit=0;
    while ((pos = line.find(" ")) != string::npos)
    {   token = line.substr(0,pos);

        line.erase(0,pos + 1);
        if (posit==1)
        {
            *x=std::stof(token);
        } else if (posit==2)
        {
            *y=std::stof(token);
        }
        posit++;
    }
}

void get_three_vals(std::string line, float *x, float *y,float *angle)
{
    size_t pos =0;
    std::string token;

    line.erase(remove(line.begin(), line.end(), ')'), line.end());

    int posit=0;
    while ((pos = line.find(" ")) != string::npos)
    {   token = line.substr(0,pos);

        line.erase(0,pos + 1);
        if (posit==1)
        {
            *x=std::stof(token);
        } else if (posit==2)
        {
            *y=std::stof(token);
        } else if (posit==3)
        {
            *angle=std::stof(token);
        }
        posit++;
    }
}

void get_one_valf(std::string line, float *width)
{   size_t pos =0;
    int posit=0;
    std::string token;

    line.erase(remove(line.begin(), line.end(), ')'), line.end());

    while ((pos = line.find(" ")) != string::npos)
    {   token = line.substr(0,pos);
        line.erase(0,pos + 1);
        if (posit==1)
        {
            *width=std::stof(token);
        }
        posit++;
    }
}

void get_one_vals(std::string line, std::string *value)
{   size_t pos =0;
    int posit=0;
    std::string token;

    line.erase(remove(line.begin(), line.end(), ')'), line.end());

    while ((pos = line.find(" ")) != string::npos)
    {   token = line.substr(0,pos);
        line.erase(0,pos + 1);
        if (posit==1)
        {
            *value=token;
        }
        posit++;
    }
}

void rotate_angle(float x,float y,float around_x,float around_y, float angle,float *newx,float *newy)
{
  x=x-around_x;
  y=y-around_y;
  angle = angle * 3.14159265/180;
  *newx=x*cos(angle)-y*sin(angle)+around_x;
  *newy=x*sin(angle)+y*cos(angle)+around_y;
}

int readedge(String filename)
{
    #ifdef DEBUG
        cout << "Edge  Cuts" << endl;
    #endif

    std::string line;
    ifstream myfile (filename);
    float x1,y1,x2,y2,width;
    float minx,miny,maxx,maxy,radius;
    int line_type;

    if (myfile.is_open())
    {

        while (! myfile.eof() )
        {
            getline (myfile,line);
            while (line[0]==' ') line.erase(0,1);  // remove initial whitespaces
            replace( line.begin(), line.end(), ')', ' '); // remove ')' from line

            // (at indicate footprint location
            if ((line.find("Edge.Cuts")!=string::npos) && (line.find("gr_")!=string::npos))
            {
                unsigned first,last=0;
                float centrex,centrey,width,height,holex,holey,angle;
                string token;

                while (last<255)
                {
                    first = line.find("(");
                    last = line.substr(first+1).find("(");
                    token = line.substr (first+1,last-first);

                    line = line.substr(last+1);

                    if (token.find("gr_line")!=string::npos) line_type=K_LINE;
                    if (token.find("gr_circle")!=string::npos) line_type=K_CIRCLE;
                    if (token.find("gr_arc")!=string::npos) line_type=K_ARC;

                    if ((token.find("start")!=string::npos) || (token.find("center")!=string::npos))
                    {
                        get_two_vals(token,&x1,&y1);
                    }

                    if ((token.find("end")!=string::npos))
                    {
                        get_two_vals(token,&x2,&y2);
                    }

                    if ((token.find("width")!=string::npos))
                    {
                        get_one_valf(token, &width);
                    }

                    if ((token.find("angle")!=string::npos))
                    {
                        get_one_valf(token, &angle);
                    }
                }

                edgecuts[edgecutpos][EDGETYPE]=line_type;
                edgecuts[edgecutpos][EDGESTARTX]=x1;
                edgecuts[edgecutpos][EDGESTARTY]=y1;
                edgecuts[edgecutpos][EDGEENDX]=x2;
                edgecuts[edgecutpos][EDGEENDY]=y2;
                edgecuts[edgecutpos][EDGEWIDTH]=width;
                edgecuts[edgecutpos][EDGEANGLE]=angle;

                edgecutpos++;

                if (line_type==K_LINE)
                {
                    minx = min(x1,x2);
                    maxx = max(x1,x2);
                    miny = min(y1,y2);
                    maxy = max(y1,y2);

                } else if ((line_type==K_ARC) | (line_type==K_CIRCLE))
                {
                    radius = sqrt((x1-x2)*(x1-x2)+(y1-y2)*(y1-y2));
                    minx = x1 - radius;
                    maxx = x1 + radius;
                    miny = y1 - radius;
                    maxy = y1 + radius;
                }

                boardminx=min(minx,boardminx);
                boardminy=min(miny,boardminy);
                boardmaxx=max(maxx,boardmaxx);
                boardmaxy=max(maxy,boardmaxy);
            }
        }
    }
    return 0;
}

// GET Vias and Pads dimension
int readviapad(String filename,String sLayer)
{
    #ifdef DEBUG
        cout << "readviapad" << endl;
    #endif

    std::string line;
    ifstream myfile (filename);
    float x1,y1,angle;

    if (myfile.is_open())
    {

        while (! myfile.eof() )
        {
            getline (myfile,line);
            while (line[0]==' ') line.erase(0,1);  // remove initial whitespaces
            replace( line.begin(), line.end(), ')', ' '); // remove ')' from line

            // (at indicate footprint location
            if ((line.find("(at ")!=string::npos) & (std::count(line.begin(), line.end(), '(')==1))
            {
                angle=0;
                if (count(line.begin(), line.end(), ' ') == 4)
                {
                    get_three_vals(line, &x1, &y1, &angle);
                    angle = (360-angle)*M_PI/180;
                }
                else
                {   angle=0;
                    get_two_vals(line,&x1,&y1);
                }
            }

            // via information
            if ((line.find("via")!=string::npos) && (line.find("layers")!=string::npos))
            {
                unsigned first,last=0;
                float centrex,centrey,width,height,holex,holey;
                string token,shape_drill;

                while (last<255)
                {
                    first = line.find("(");
                    last = line.substr(first+1).find("(");
                    token = line.substr (first+1,last-first);

                    if (token.substr(0,2)=="at") get_two_vals(token,&centrex,&centrey);
                    if (token.substr(0,4)=="size") get_one_valf(token,&width);
                    if (token.substr(0,5)=="drill") get_one_valf(token,&holex);

                    if (token.substr(0,5)=="layer")
                    {
                        holey=holex;
                        height=width;
                        ipadseg[ipadpos][PADTYPE]=K_CIRCLE;
                        ipadseg[ipadpos][LOCX]=centrex;
                        ipadseg[ipadpos][LOCY]=centrey;
                        ipadseg[ipadpos][SIZEX]=width;
                        ipadseg[ipadpos][SIZEY]=height;
                        ipadseg[ipadpos][HOLEX]=holex;
                        ipadseg[ipadpos][HOLEY]=holey;
                        ipadpos++;
                    }
                    line = line.substr(last+1);
                }
            }

            // pad information
            if ((line.find("pad")!=string::npos) && (line.find("layers")!=string::npos))
            if ((line.find(sLayer)!=string::npos) || (line.find("*.Cu")!=string::npos))
            {

                #ifdef DEBUG
                    cout << line << endl;
                #endif

                unsigned first,last=0,padtype,second;
                float centrex,centrey,width,height,holex,holey,cx,cy,rratio,angle2;
                string token,shape_drill;

                while (last<255)
                {
                    first = line.find("(");
                    last = line.substr(first+1).find("(");
                    token = line.substr (first+1,last-first);

                    #ifdef DEBUG
                        cout << token << endl;
                    #endif

                    if (token.find("pad")!=string::npos)
                    {   padtype=K_CIRCLE;
                        if (token.find("oval")!=string::npos) padtype=K_ELLPSE;
                        if (token.find("rect")!=string::npos) padtype=K_RECT;
                        if (token.find("round")!=string::npos) padtype=K_ROUNDRECT;
                    }

                    if (token.substr(0,2)=="at")
                    {
                        if (std::count(token.begin(), token.end(), ' ')==4)
                        {   get_two_vals(token,&cx,&cy);
                            angle2= 0;
                        } else
                        {
                            get_three_vals(token,&cx,&cy,&angle2);
                        }

                        //rotate_angle(cx,cy,x1,y1,angle,&centrex,&centrey);
                        centrex=cx*cos(angle)-cy*sin(angle)+x1;
                        centrey=cx*sin(angle)+cy*cos(angle)+y1;
                        //cout << token << " " << angle << ":" << angle2 << endl;
                    }

                    if (token.substr(0,4)=="size")
                    {
                        get_two_vals(token,&cx,&cy);
                        width  = cx; //abs(cx*cos(angle)-cy*sin(angle));
                        height = cy; // abs(cx*sin(angle)+cy*cos(angle));

                        //cout << token << " " << cx << ":" << cy << endl;
                    }

                    if (token.substr(0,5)=="drill")
                    {
                        if (count(token.begin(), token.end(), ' ')==3)
                        {   get_one_valf(token,&holex);
                            holey=holex;
                        }
                        else
                        {
                            first = line.find(" ");
                            token = line.substr (first+1);
                            second = line.find(" ");
                            shape_drill = line.substr(first+1,second-2);
                            get_two_vals(token,&holex,&holey);
                        }
                    }

                    if (token.substr(0,9)=="roundrect")
                    {
                        get_one_valf(token,&rratio);
                    }

                    if (token.substr(0,5)=="layer")
                    {
                            // Nothing
                    }

                    line = line.substr(last+1);
                }
                // End of reading one line
                if (shape_drill!="oval")
                {
                  holex = min(holex,holey); // Lets take the smallest value since we dont know how to make ellipse gcode
                  holey=holex;
                }

                ipadseg[ipadpos][PADTYPE]=padtype;
                ipadseg[ipadpos][LOCX]=centrex;
                ipadseg[ipadpos][LOCY]=centrey;
                ipadseg[ipadpos][SIZEX]=width;
                ipadseg[ipadpos][SIZEY]=height;
                ipadseg[ipadpos][HOLEX]=holex;
                ipadseg[ipadpos][HOLEY]=holey;
                ipadseg[ipadpos][RRATIO]=rratio;
                ipadseg[ipadpos][ANGLE]=angle2;


                boardminx=min(centrex,boardminx);
                boardminy=min(centrey,boardminy);
                boardmaxx=max(centrex,boardmaxx);
                boardmaxy=max(centrey,boardmaxy);
                ipadpos++;

            }
        }

        cout << ipadpos << " vias/pads read." << endl;
    }
    else
    {
        #ifdef DEBUG
            cout << "Problem File" << endl;
        #endif
        return ENOENT;
    }
    return 0;
}



// GET Tracks
int readkicad(String filename, String sLayer)
{
    #ifdef DEBUG
        cout << "readkicad" << endl;
    #endif

    std::string line;
    std::string layer;
    float x1,y1,x2,y2,width;

    ifstream myfile (filename);
    if (myfile.is_open())
    {
        while (! myfile.eof() )
        {
            getline (myfile,line);
            if (line.find("segment")!=string::npos)
            {
                size_t pos =0;
                std::string token;
                while ((pos = line.find("(")) != string::npos)
                {
                    token = line.substr(0,pos);

                    if (token.rfind("start",0) == 0)
                    {
                        get_two_vals(token,&x1,&y1);
                    }
                    if (token.rfind("end",0) == 0)
                    {
                        get_two_vals(token,&x2,&y2);
                    }
                    if (token.rfind("width",0) == 0)
                    {
                        get_one_valf(token,&width);
                    }
                    if (token.rfind("layer",0) == 0)
                    {
                        get_one_vals(token,&layer);

                        // Only look at Bottom PCB
                        // find boardminx,boardminy and boardmaxx,boardmaxy by selecting
                        // min and max of track segments co-ordinates

                        if (layer == sLayer) //"B.Cu" or "F.Cu"
                        {
                            linesegment[linesegpos][LINEX1]=x1;
                            linesegment[linesegpos][LINEY1]=y1;
                            linesegment[linesegpos][LINEX2]=x2;
                            linesegment[linesegpos][LINEY2]=y2;
                            linesegment[linesegpos][LINEWIDTH]=width;

                            boardminx=min(x1,boardminx);
                            boardminx=min(x2,boardminx);

                            boardminy=min(y1,boardminy);
                            boardminy=min(y2,boardminy);

                            boardmaxx=max(x1,boardmaxx);
                            boardmaxx=max(x2,boardmaxx);

                            boardmaxy=max(y1,boardmaxy);
                            boardmaxy=max(y2,boardmaxy);

                            linesegpos++;
                        }
                    }

                    line.erase(0,pos + 1);
                }

            }

        }
        myfile.close();

        cout << linesegpos << " track segments read." << endl;
    }
    else
    {
        return ENOENT;
    }


  return 0;
}

void scale_down(unsigned int pxmm)
{
    #ifdef DEBUG
        cout << "scaledown" << endl;
    #endif

    boardmaxx+=2; // Increment slightly boardmaxx
    boardminx-=2; // Decrement slightly boardinx
    boardmaxy+=2; // Increment slightly boardmaxy
    boardminy-=2; // Decrement slightly boardminy

    image_width = int(boardmaxx-boardminx)*pxmm;
    image_height = int(boardmaxy-boardminy)*pxmm;

    /* Store via and pad as
            PADTYPE,
            CENTRE X, CENTRE Y,
            X SIZE OF PAD, Y SIZE OF PAD
            X SIZE of DRILL HOLE, Y SIZE of DRILL HOLE

        move origin of pcb board to boardminx,boardminy
        scale up using Pixels per mm
    */
    for (unsigned int li=0; li<ipadpos; li++)
    {
        ipad[li][PADTYPE]   = int(ipadseg[li][PADTYPE]);
        ipad[li][LOCX]      = int((ipadseg[li][LOCX]-boardminx)*pxmm);
        ipad[li][LOCY]      = int((ipadseg[li][LOCY]-boardminy)*pxmm);

        ipad[li][SIZEX]     = int((ipadseg[li][SIZEX]*pxmm/2));
        ipad[li][SIZEY]     = int((ipadseg[li][SIZEY]*pxmm/2));

        ipad[li][HOLEX]     = int((ipadseg[li][HOLEX])*pxmm);
        ipad[li][HOLEY]     = int((ipadseg[li][HOLEY])*pxmm);
        ipad[li][ANGLE]     = int((ipadseg[li][ANGLE]));

    }

    /* Store track segment line
        X1,Y1,X2,Y2, Width

        move origin of pcb board to boardminx,boardminy
        scale up using Pixels per mm
    */

    for (unsigned int li=0; li<linesegpos; li++)
    {
        iline[li][0]=int((linesegment[li][LINEX1]-boardminx)*pxmm);
        iline[li][2]=int((linesegment[li][LINEX2]-boardminx)*pxmm);

        iline[li][1]=int((linesegment[li][LINEY1]-boardminy)*pxmm);
        iline[li][3]=int((linesegment[li][LINEY2]-boardminy)*pxmm);

        iline[li][4]=int((linesegment[li][LINEWIDTH])*pxmm);
    }

    /* Store edge Cuts
        X1,Y1,X2,Y2, Width

        move origin of pcb board to boardminx,boardminy
        scale up using Pixels per mm
    */

    for (unsigned int li=0; li<edgecutpos; li++)
    {
        iedgecuts[li][EDGETYPE]   = int(edgecuts[li][EDGETYPE]);

        iedgecuts[li][EDGESTARTX] = int((edgecuts[li][EDGESTARTX]-boardminx)*pxmm);
        iedgecuts[li][EDGEENDX]   = int((edgecuts[li][EDGEENDX]-boardminx)*pxmm);

        iedgecuts[li][EDGESTARTY] = int((edgecuts[li][EDGESTARTY]-boardminy)*pxmm);
        iedgecuts[li][EDGEENDY]   = int((edgecuts[li][EDGEENDY]-boardminy)*pxmm);

        iedgecuts[li][EDGEWIDTH]  = int((edgecuts[li][EDGEWIDTH])*pxmm);
    }

}

//Create image
void showpic()
{   cv::Scalar color = cv::Scalar(255.0, 255.0, 255.0);
    cv::Point2f vertices2f[4];
    cv::Point vertices[4];

    #ifdef DEBUG
        cout << "showpic" << endl;
    #endif
    Mat city = Mat::zeros(Size(image_width,image_height),CV_8UC3);

    for (int i=0; i<linesegpos; i++)
    {
        line(city,Point2d(iline[i][0],iline[i][1]),Point2d(iline[i][2],iline[i][3]),Vec3b(255,255,255),iline[i][4]);
    }

    for (int i=0; i<ipadpos; i++)
    {
        if (ipad[i][0]==K_RECT)
        {
            cv::RotatedRect rotatedRectangle(Point2f(ipad[i][LOCX],ipad[i][LOCY]),
                                             Size2f(ipad[i][SIZEX] << 1,ipad[i][SIZEY] << 1),
                                              -(double)ipad[i][ANGLE]);
            rotatedRectangle.points(vertices2f);
            for(int i = 0; i < 4; ++i) vertices[i] = vertices2f[i];

             cv::fillConvexPoly(city,vertices,4,color);
        } else
        {
            cv::RotatedRect rotatedRectangleE(Point2f(ipad[i][LOCX],ipad[i][LOCY]),
                                             Size2f(ipad[i][SIZEX] << 1,ipad[i][SIZEY] << 1),
                                              -(double)ipad[i][ANGLE]);
            cv::ellipse(city,rotatedRectangleE,color,-1);
        }
    }

    imwrite( "map.png", city );

    // Create EDGE MASK
    if (edgecutpos>0)
    {

        city = Mat::zeros(Size(image_width,image_height),CV_8UC3);
        int x1,y1,x2,y2,radius,width, startAngle, endAngle;
        float dx,dy;

        for (int i=0; i<edgecutpos; i++)
        {   width=1;

            width = iedgecuts[i][EDGEWIDTH];
            width = max (2,width);

            if (iedgecuts[i][EDGETYPE]==K_LINE)
            {
                line(city,Point2d(iedgecuts[i][EDGESTARTX],iedgecuts[i][EDGESTARTY]),
                    Point2d(iedgecuts[i][EDGEENDX],iedgecuts[i][EDGEENDY]),
                    color,width);

            } else if (iedgecuts[i][EDGETYPE]==K_CIRCLE) {
                x1=iedgecuts[i][EDGESTARTX];
                x2=iedgecuts[i][EDGEENDX];
                y1=iedgecuts[i][EDGESTARTY];
                y2=iedgecuts[i][EDGEENDY];
                radius = sqrt((x1-x2)*(x1-x2)+(y1-y2)*(y1-y2));
                circle(city, Point2d(x1,y1),radius,color,width);
            } else if (iedgecuts[i][EDGETYPE]==K_ARC) {
                x1=iedgecuts[i][EDGESTARTX];
                x2=iedgecuts[i][EDGEENDX];
                y1=iedgecuts[i][EDGESTARTY];
                y2=iedgecuts[i][EDGEENDY];
                radius = sqrt((x1-x2)*(x1-x2)+(y1-y2)*(y1-y2));

                dy = y2-y1;
                dx = x2-x1;
                if (dx!=0)
                {
                  startAngle =  atan(dy/dx)*180/3.14159265;

                  if (dx<0)
                  {
                    startAngle = 180+startAngle;
                  }
                  endAngle = startAngle+edgecuts[i][EDGEANGLE];

                } else
                {
                  if (dy>0) startAngle = 90; else startAngle = 180;
                  endAngle = startAngle-edgecuts[i][EDGEANGLE];

                }

                ellipse(city,Point2d(x1,y1),Size2f(radius,radius),0,startAngle,endAngle,color,width);

            }
        }

        int i=0;

        while ((city.at<Vec3b>(Point(i,image_height >> 1))==Vec3b(0,0,0)) && (i<image_width)) i++; //find 1st edge
        x1=i;

        while ((city.at<Vec3b>(Point(i,image_height >> 1))!=Vec3b(0,0,0)) && (i<image_width)) i++; // traverse edge
        while ((city.at<Vec3b>(Point(i,image_height >> 1))==Vec3b(0,0,0)) && (i<image_width)) i++; // find 2nd edge
        x2=i;

        if (x2<image_width)
        {
            floodFill(city, Point2d((x2+x1) >> 1,image_height >> 1), Scalar(255,255,255));
            imwrite( "mask.png", city );
        }
        else
        {
            cout << "Error parsing Edge Cut shape." << endl;
        }
    }

}

//pseudo random color pick
cv::Scalar get_colour_from_pool()
{
    static int seed=21312;

    while (color_pool[seed]==true)
    {
        seed=seed+7236; // not very random but colorful
        if (seed>31*31*31)
        {
            seed-=31*31*31;
        }
    }

    color_pool[seed]=true;
    int r=(seed & 31744) >> 7;
    int g=(seed & 992) >> 2;
    int b=(seed & 31) << 3;

    return CV_RGB(r,g,b);
}

void colorize_tracks(Mat &image)
{
    CV_Assert(image.depth() == CV_8U);

    int nRows = image.rows;
    int nCols = image.cols;
    int channels = image.channels();

    //Init color pool
    for (int r=1; r<31*31*31; r++)
        color_pool[r]=true;

    for (int r=1; r<32; r++)
    for (int g=1; g<32; g++)
    for (int b=1; b<32; b++)
        color_pool[(r << 10) + (g << 5) +b]=false;

    //Colourize tracks with different colours
    for (int r =0; r<nRows; r++)
    {
        for (int c =0; c<nCols; c++)
        {   Vec3b colour = image.at<Vec3b>(Point(c, r));

            if(colour.val[0]==255 && colour.val[1]==255 && colour.val[2]==255)
            {
                floodFill(image,Point(c,r), get_colour_from_pool() );
            }
        }

    }
}

// Dilate with colours
bool track_dilate(Mat &image,Mat &newimage, int sRow,int nRows,int sCol,int nCols)
{
    typedef unsigned char byte;
    Mat dimage[3];
    byte rr[9],gg[9],bb[9];
    int tb[32768];
    char cr,cg,cb;
    int index,tmaxc;

    bool change=false;

    {
    
    for (int r=sRow; r<(nRows-1); r++)
        {
            for (int c=sCol; c<(nCols-1); c++)
            {
                if ((uchar) image.at<Vec3b>(r,c)[2]==0) // if focus pixel is background
                {
                    Mat roi = image(Rect(c-1,r-1,3,3));
                    
                    /// Are there any coloured pixels round focus?
                    split(roi,dimage);
                    memcpy(bb,dimage[2].data, 9*sizeof(byte)); // no edge has Blue=0
                    bool edge=false;
                    for (int lll=0; lll<9; lll++) if ((int)bb[lll]>0) {edge=true;}

                    /// if Yes its and edge
                    if (edge)
                    {
                        memcpy(rr,dimage[0].data, 9*sizeof(byte)); // get rest of pixel values red
                        memcpy(gg,dimage[1].data, 9*sizeof(byte)); // get rest of pixel values blue
                        memset(tb,0,32768*sizeof(int));

                        std::vector<int> colour_list = {};

                        // Build a list of colours around the focus pixel
                        for (int l=0; l<9; l++)
                        {
                            if (l!=4) // Go through all pixels except focus
                            {
                                index = ((rr[l] & 248) << 7) | ((gg[l] & 248) << 2) | (bb[l] >> 3);

                                tb[index]=tb[index]+1;

                                if (std::find(std::begin(colour_list), std::end(colour_list), index) == std::end(colour_list))
                                {
                                    colour_list.insert(colour_list.begin(),index);
                                }

                            }
                        }

                        tb[0]=0; // which is not background
                        index=0;

                        // Selected colour = colour of  most neighbours pixels with the same colour
                        for (std::vector<int>::iterator it = colour_list.begin() ; it != colour_list.end(); ++it)
                        {
                            if (tb[index]<tb[*it]) { index=*it;}
                        }


                        if ((int) tb[index]>0)
                        {
                            cr=((index & 31744) >> 7);
                            cg=((index & 992) >> 2);
                            cb=((index & 31) << 3);

                            // Change Focus pixel colour to Selected Colour
                            newimage.at<Vec3b>(Point(c, r))=Vec3b(cr,cg,cb); 
                        }

                        change=true;
                    }
                }
            }

        }
    }
    return change;
}

void ScanImageAndReduceC(Mat &I,Mat &image)
{
    // accept only char type matrices
    CV_Assert(I.depth() == CV_8U);
    I.copyTo(image);

    int nRows = I.rows;
    int nCols = I.cols;
    int channels = I.channels();

    String progress[4]= {"|...",".|..","..|.","...|"};
    int progressbar=0;

    cout << "Progress: " << progress[progressbar++] << "\r" << std::flush;
    if (progressbar==4) progressbar=0;

    
    bool change=true;
    Mat newimage;

    while (change)
    {
        cout << "Progress: " << progress[progressbar++] << "\r" << std::flush;
        if (progressbar==4) progressbar=0;

        image.copyTo(newimage);
        change = track_dilate(image, newimage,1, nRows,  1,nCols); 
        //change = track_dilate(image, newimage,              1, nRows >> 1,  1,              (nCols >> 1)); 
        //change = track_dilate(image, newimage, (nRows >> 1)-1, nRows,       1,              (nCols >> 1));
        //change = track_dilate(image, newimage,              1, nRows >> 1,  (nCols >> 1)-1, nCols       );
        //change = track_dilate(image, newimage, (nRows >> 1)-1, nRows,       (nCols >> 1)-1, nCols       );
        
        newimage.copyTo(image);
        
    }
};

int getcontourexpansion(String sLayer)
{
    #ifdef DEBUG
            cout << "getcontourexpansions" << endl;
    #endif

    Mat image,city,output;
    bool file_is_ready = false;

    if (!file_is_ready)
    {
        String imageName( "map.png" ); // by default
        image = imread( samples::findFile( imageName ), IMREAD_COLOR ); // Read the file
        if( image.empty() )                      // Check for invalid input
        {
            cout <<  "Could not open or find the image" << std::endl ;
            return -1;
        }

        city = Mat::zeros(Size(image.cols,image.rows),CV_8UC3);
        colorize_tracks(image);            // Colorize tracks
        ScanImageAndReduceC(image,city);   // Dilate tracks until all tracks meet at middle. Then find edge.


        String maskName = "mask.png";
        std::ifstream maskfile(maskName);

        if (maskfile.good())
        {
            Mat mask = imread( samples::findFile( maskName ), IMREAD_COLOR ); // Read the file
            if( image.empty() )                      // Check for invalid input
            {
                Mat mask(Size(image.cols,image.rows),CV_8UC3, Scalar(255,255,255));
            }

            bitwise_and(city,mask,city);                                    // Mask out Edge Cuts
        }
        imwrite( "cpp_image.png", city );  // Output to cpp_image.png
    }
    else
    {
        city = imread("cpp_image.png");
    }

    // Edge detection
    Canny( city, detected_edges, 50, 150, 7 );

    // Dilate and Erode lines so that close lines get attached.
    Mat element = getStructuringElement( MORPH_RECT,Size( 2*5 + 1, 2*5+1 ),Point( 5, 5 ) );
    dilate( detected_edges, detected_edges, element );
    erode( detected_edges, detected_edges, element );

    imwrite( "trace.png", detected_edges );

    return 0;
}


void gcode_print(String gout)
{
    gcode = gcode + gout + "\n";
}

/*
    Find contours of the edges between the track expansion.
    Use contour and segment in lines.
 */
int tracecontourexpansion(unsigned int pxmm, string sLayer)
{
    #ifdef DEBUG
        cout << "tracecontourexpansion" << endl;
    #endif

    // threshold function
    Mat outImg,drImg;
    bool found = true;
    vector<vector<Point> > contours;
    vector<Point> smallcontours;
    vector<Vec4i> hierarchy;

    float xflip = 1.0;
    if (sLayer=="B.Cu")
    {
        xflip=-1.0;
    }

    drImg = Mat::zeros(Size(detected_edges.cols,detected_edges.rows),CV_8UC3);
    Scalar color( 255, 0, 0 );
    int cnt = 500;

    gcode_print("G21"); // Metric
    gcode_print("G90"); // Absolute
    gcode_print("G94"); // Feed per minute

    while (found)
    {
        // Get all contours from Input Image
        findContours( detected_edges, contours, hierarchy, RETR_TREE, CHAIN_APPROX_SIMPLE, Point(0, 0) );

        bool first = true;
        unsigned int no_lines = 0;
        Point j,k;

        // remove First Contour by tracing lines on Input Image
        for (Point i: contours[0] )
        {
            if (first)
            {
                first=false;
                j=i;
                k=i;
            }

            line(detected_edges, i, j, Scalar(0,0,0), 2, LINE_4);
            j=i;

            no_lines++;
        }
        line(detected_edges, k, j,  Scalar(0,0,0), 2, LINE_4);

        // build gcode from Approximate contour
        approxPolyDP(Mat(contours[0]),smallcontours,1,true);

        // gcode blockheader
        gcode_print("G0 Z2"); // dont need this in laser
        gcode_print("(Block-name: block "+to_string(blockcounter)+")\n(Block-expand: 0)\n(Block-enable: 1)");
        blockcounter++;
        gcode_print("G01 F120"); // Feedrate 120 mm/min
        gcode_print("G0 X"+to_string(xflip*smallcontours[0].x/pxmm)+" Y"+to_string(-1.0*smallcontours[0].y/pxmm));
        gcode_print("M04 S1000");

        // gcode line segments
        first = true;
        for (Point i: smallcontours)
        {
            if (first)
            {
                first=false;
                j=i;
                k=i;
            }
            else
            {
                gcode_print("G1 X"+to_string(xflip*i.x/pxmm)+" Y"+to_string(-1.0*i.y/pxmm));
            }
        }
        gcode_print("G1 X"+to_string(xflip*smallcontours[0].x/pxmm)+" Y"+to_string(-1.0*smallcontours[0].y/pxmm));
        gcode_print("M05\n ");

        // if no more contours then stop
        if (contours.size()<2) found = false;
        cnt--;
        if (cnt==0) found = false;
    }

    return 0;
}

// Use original kicad drill holes
// Assumption holes are circular

void trace_drillholes(unsigned int pxmm, string sLayer)
{
    #ifdef DEBUG
        cout << "drillholes" << endl;
    #endif
    float x1,x2,yy,r;
    float hx,hy;
    float x11,x12,x21,x22,y11,y12,y21,y22;
    float c1x,c1y,c_r, c2x,c2y,angle;

    float xflip = 1.0;
    if (sLayer=="B.Cu")
    {
        xflip=-1.0;
    }

    for (unsigned int li=0; li<ipadpos; li++)
    {
        x1=(xflip*ipad[li][LOCX]+ipad[li][HOLEX]/2)/pxmm;
        x2=(xflip*ipad[li][LOCX]/pxmm);
        yy=(-1.0*ipad[li][LOCY]/pxmm);
        r=(1.0*ipad[li][HOLEX]/(2*pxmm));



        // GCODE Block init
        gcode_print("(Block-name: blockvia "+to_string(blockcounter)+")\n(Block-expand: 0)\n(Block-enable: 1)");
        blockcounter++;
        gcode_print("G01 F120"); // Feedrate 120 mm/min

        //## oval
        if ( ipad[li][HOLEX] > ipad[li][HOLEY])
        {
            angle = 180-1.0* ipadseg[li][ANGLE];
            hx = (ipad[li][HOLEX]- ipad[li][HOLEY])/(2.0*pxmm);
            hy = (1.0*ipad[li][HOLEY])/(2*pxmm);

            rotate_angle(x2-hx, yy-hy, x2, yy, angle, &x11, &y11);
            rotate_angle(x2+hx, yy-hy, x2, yy, angle, &x12, &y12);
            rotate_angle(x2-hx, yy+hy, x2, yy, angle, &x21, &y21);
            rotate_angle(x2+hx, yy+hy, x2, yy, angle, &x22, &y22);

            rotate_angle(x2-hx, yy, x2, yy, angle, &c1x, &c1y);
            rotate_angle(x2+hx, yy, x2, yy, angle, &c2x, &c2y);

            gcode_print("G0 X"+to_string(x11)+" Y"+to_string(y11));
            gcode_print("M04 S1000");
            gcode_print("G1 X"+to_string(x12)+" Y"+to_string(y12));

            gcode_print("G03 X"+to_string(x22)+" Y"+to_string(y22)+" I"+to_string(c2x-x12)+"J"+to_string(c2y-y12)+" F120");
            gcode_print("G1 X"+to_string(x21)+" Y"+to_string(y21));
            gcode_print("G03 X"+to_string(x11)+" Y"+to_string(y11)+" I"+to_string(c1x-x21)+"J"+to_string(c1y-y21)+" F120");


        } else if ( ipad[li][HOLEX] < ipad[li][HOLEY])
        {
            hy = -(ipad[li][HOLEY]- ipad[li][HOLEX])/(2.0*pxmm);
            hx = (1.0*ipad[li][HOLEX])/(2*pxmm);

            angle = 180-1.0*ipadseg[li][ANGLE];
            rotate_angle(x2-hx, yy-hy, x2, yy, angle, &x11, &y11);
            rotate_angle(x2+hx, yy-hy, x2, yy, angle, &x21, &y21);
            rotate_angle(x2-hx, yy+hy, x2, yy, angle, &x12, &y12);
            rotate_angle(x2+hx, yy+hy, x2, yy, angle, &x22, &y22);

            rotate_angle(x2, yy-hy, x2, yy, angle, &c1x, &c1y);
            rotate_angle(x2, yy+hy, x2, yy, angle, &c2x, &c2y);

            gcode_print("G0 X"+to_string(x11)+" Y"+to_string(y11));
            gcode_print("M04 S1000");
            gcode_print("G1 X"+to_string(x12)+" Y"+to_string(y12));

            gcode_print("G02 X"+to_string(x22)+" Y"+to_string(y22)+" I"+to_string(c2x-x12)+"J"+to_string(c2y-y12)+" F120");
            gcode_print("G1 X"+to_string(x21)+" Y"+to_string(y21));
            gcode_print("G02 X"+to_string(x11)+" Y"+to_string(y11)+" I"+to_string(c1x-x21)+"J"+to_string(c1y-y21)+" F120");
        }
        else
        {
          // Goto rightmost
          gcode_print("G0 X"+to_string(x1)+" Y"+to_string(yy));
          gcode_print("M04 S1000");

          // Draw circle
          gcode_print("G02 X"+to_string(x1)+" I-"+to_string(r)+" F120");
      }
    }
}

int main(int argc, char** argv)
{
    String filename,sLayer="B.Cu";
    int flags, opt;
    bool process=false;

    if ((argc<2) | (argc>4))
    {
        cout << "Usage: " << argv[0] << " <filename> " <<  endl;
        cout << "\tOption: -m         Process map.png directly" << endl;
        cout << "\t        -f         Process Front Copper Layer." << endl;
        cout << "\t        -p<pxmm>   Change pixels per mm (default 30)" << endl;
        cout << endl;

        if (argc>4) return E2BIG;
        return EINVAL;
    }

    if ((argc>2) & (argv[1][0]=='-')) // first parameter should be a filename
    {
        cout << "First parameter should be a KiCad PCBnew filename." << endl;
        return ENOENT;
    }

    // Check filename exists
    filename = argv[1];
    ifstream the_file(filename);
    if (!the_file.is_open())
    {
        cout << "Could not open file " << filename << "\n";
        return ENOENT;
    }
    the_file.close();

    if (argc>2)
    {
        for (int i=2; i<argc; i++)
        {   //cout << i << " " << argv[i] << endl;
            if (argv[i][0]=='-')
            {
                switch (argv[i][1])
                {
                    case 'f': // Process Front Copper instead of Bottom Copper
                        cout << "Processing Front Copper Layer." << endl;
                        sLayer = "F.Cu";
                    break;

                    case 'm':  // Process map.png directly
                        if (strlen(argv[i])!=2)
                        {
                            cout << "Invalid Argument for " << argv[0] << endl;
                            return ENOENT;
                        }
                        process=true;
                    break;

                    case 'p': // Pixels per mm
                        if (strlen(argv[i])>2)
                        {
                            String str_pxmm = String(argv[i]).substr(2);
                            if (is_number(str_pxmm))
                            {
                                cout << "Pixels per mm: " << str_pxmm << endl;
                                try
                                {
                                    pixels_per_mm = stoi(str_pxmm);
                                    if (pixels_per_mm>1000)
                                    {
                                        cout << "Pixels per mm range 1..100" << endl;
                                        return ENOENT;
                                    }
                                }
                                catch (std::invalid_argument const &e)
                                {
                                    cout << "Bad input " << argv[i] << endl;
                                    return ENOENT;
                                }
                                catch (std::out_of_range const &e)
                                {
                                    cout << "Integer overflow." << endl;
                                    return ENOENT;
                                }
                            } else
                            {
                                cout << "Invalid Argument for " << argv[0] << endl;
                                return ENOENT;
                            }
                        }
                        else
                        {
                            cout << "Invalid Argument for " << argv[0] << endl;
                            return ENOENT;
                        }
                    break;

                    default:
                        cout << "Invalid Argument for " << argv[0] << endl;
                        return ENOENT;
                }

            }
            else
            {
                cout << "Invalid Argument for " << argv[0] << endl;
                return ENOENT;
            }

        }
    }

    if (process==false)
    {
        remove("mask.png"); // remove mask.png

        readkicad(filename, sLayer);  // read pcb tracks

        readviapad(filename, sLayer); // read pad/via information

        readedge(filename); //read Edge Cuts

        if (ipadpos==0) return 0;
        scale_down(pixels_per_mm);

        showpic();    // output bw image map.png
    }

    getcontourexpansion(sLayer); // Expand tracks and find edge boundary of expansion
    tracecontourexpansion(pixels_per_mm, sLayer);
    trace_drillholes(pixels_per_mm, sLayer); //  via/pad holes to gcode

    // Output gcode to kic.gcode
    std::ofstream out("kic.gcode");
    out << gcode;
    out.close();

    cout << "kic.gcode written." << endl;

    return 0;
}
