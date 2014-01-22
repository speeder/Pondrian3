#include <stdio.h> //this is so we can use fprintf to print errors to STDERR
#include <math.h> //needed for random, floor, ceil, etc...
#include <time.h> //needed to seed the random generator
#include <allegro5/allegro.h>
#include <allegro5/allegro_primitives.h> //this allegro addon draws vectors to the screen, like rectnagles, circles...
#include <allegro5/allegro_audio.h> //this allegro addon plays audio
#include <allegro5/allegro_acodec.h> //this allegro addon loads audio, note that the allegro_audio cannot load audio on its own


#define SCREEN_W 600 //this is the playfield size
#define SCREEN_H 600
#define PLAYER_H_WIDTH 50 //this is HALF of player size, because to render the initial point is center - half and the end point is center + half
#define PLAYER_H_HEIGHT 12 //same as above
#define BALL_SIZE_H 16 //same as above
#define BLACK_LINE_WIDTH 4 //width of line decoration, affects gameplay slightly for aesthetic reasons
#define BUFFER 14 //mininum distance between player paddle and wall
enum 
{
	LEFT_BUFFER = PLAYER_H_WIDTH+BUFFER,
	RIGHT_BUFFER = SCREEN_W-LEFT_BUFFER
}; //C has no true constants beside macro and enum tricks, we used a enum trick here because a macro would just expand to some crazy math expressions instead
const float FPS = 60; //this is to calculate how fast we will update the game using a timer

#define point_between_points(p1, p2, p3) ((p1) > (p2) && (p1) < (p3)) //macro created so code is easier to read later

//ball static variables
//to avoid passing boatloads of arguments to the init_ball() function, I decided to put all ball variables here instead.
int ballX;
int ballY;
int ballSpeedX;
int ballSpeedY;
int ballx1;
int ballx2;
int bally1;
int bally2;
int wooble;
int wooblespeed=0;

void init_ball()
{
	srand(time(NULL));
	if(rand()%100>49) ballSpeedY = 3;
	else ballSpeedY = -3;
	ballSpeedX = rand()%2+1;
	if(rand()%100>49) ballSpeedX = -ballSpeedX;
	ballX = SCREEN_W/2;
	ballY = SCREEN_H/2;
	wooble = 10;
	wooblespeed = 0;
}

int main(int argc, char **argv) //we don't use these parameters, and this triggers a warning, but a Allegro macro need them.
{
	ALLEGRO_DISPLAY *display = NULL; //this refers to the window (if in windowed mode) + contents
	ALLEGRO_EVENT_QUEUE *event_queue = NULL; //we need a event queue to well, queue events.
	ALLEGRO_TIMER *timer = NULL; //this timer tracks how often we update the game
	ALLEGRO_SAMPLE *bounceSample=NULL;
	ALLEGRO_SAMPLE *wonSample=NULL;
	ALLEGRO_SAMPLE *lostSample=NULL;
	ALLEGRO_MONITOR_INFO monitorInformation; //this is the information of screens positions (if multi-screen setup) and their resolution
	int monitorWidth;
	int monitorHeight;
	double scalex; //actually values here are integer after calculation, it is the scale between playfield and actual screen size
	double scaley; //same as above
	double translationx; //sabe as above, but for camera panning instead, to center the game on the user screen
	double translationy;
	double sampleSpeed; //this controls how fast and high pitched sound effects play
	
	//player variables
	int player1x = 1;
	int player1y = 1;
	int player2x = 1;
	int player2y = 1;
	int delta;
	int aiMaxSpeed = 2; //this is how much pixels per frame the AI can move toward the ball, controls the actual difficulty of the game
	int player1Score = 0;
	int player2Score = 0;
	bool redraw = true; //when this is true, the screen will update.
	
	
	if(!al_init()) 
	{
		fprintf(stderr, "failed to initialize allegro!\n"); //should not happen, but who knows...
		return -1;
	}
	
	if(!al_install_mouse())
	{
		fprintf(stderr, "failed to initialize the mouse!\n");
		return -1;
	}
	
	timer = al_create_timer(1.0 / FPS); //function take seconds as argument, thus we want to update 1 second / frames per second.
	if(!timer)
	{
		fprintf(stderr, "failed to create timer!\n");
		return -1;
	}
	
	int counter = 0;
	while(counter < al_get_num_video_adapters()) //the user might have more than once adapter, not necessarily in use
	{
		if(!al_get_monitor_info(counter, &monitorInformation)) fprintf(stderr, "failed to get monitor info for some reason!\n");
		if(monitorInformation.x1 == 0 && monitorInformation.x2 > 0) break; //if we found a valid "center" monitor we use that info.
		++counter;
	}
	
	monitorWidth = monitorInformation.x2-monitorInformation.x1;
	monitorHeight = monitorInformation.y2-monitorInformation.y1;
	
	al_set_new_display_option(ALLEGRO_VSYNC, 1, ALLEGRO_SUGGEST); //this is to avoid tearing on fullscreen, not needed on window modes
	al_set_new_display_flags(ALLEGRO_OPENGL | ALLEGRO_FULLSCREEN); //I am using fullscreen because in windowed mode is easy to click outside the game and switch to something else
	display = al_create_display(monitorWidth, monitorHeight);
	if(!display)
	{
		fprintf(stderr, "failed to create display!\n");
		al_destroy_timer(timer);
		return -1;
	}
	
	al_hide_mouse_cursor(display); //the cursor is distracting, so I disabled it
	
	scalex=floor((double)monitorWidth/(double)SCREEN_W);
	scaley=floor((double)monitorHeight/(double)SCREEN_H);
	if(scalex < scaley) scaley = scalex;
	else scalex = scaley;
	translationx=(monitorWidth-SCREEN_W*scalex)/2;
	translationy=(monitorHeight-SCREEN_H*scaley)/2;
	
	ALLEGRO_TRANSFORM trans;
	al_identity_transform(&trans);
	al_scale_transform(&trans, scalex, scaley);
	al_translate_transform(&trans, translationx, translationy);
	al_use_transform(&trans); //the idea here is allow screens bigger than the game still play it confortably
	
	event_queue = al_create_event_queue();
	if(!event_queue)
	{
		fprintf(stderr, "failed to create event_queue!\n");
		al_destroy_display(display);
		al_destroy_timer(timer);
		return -1;
	}
	
	if(!al_install_audio())
	{
		fprintf(stderr, "failed to initialize audio!\n");
		return -1;
	}
	
	if(!al_init_acodec_addon())
	{
		fprintf(stderr, "failed to initialize audio codecs!\n");
		return -1;
	}
	
	if(!al_reserve_samples(3)) //we reserve 3 samples, this is only needed when using Allegro default mixer, and we are using it.
	{
		fprintf(stderr, "failed to reserve samples!\n");
		return -1;
	}
	
	bounceSample = al_load_sample( "square.wav" );
	wonSample = al_load_sample("won.wav");
	lostSample = al_load_sample("lost.wav");
	
	if(!al_init_primitives_addon())
	{
		fprintf(stderr, "failed to initialize primitives rendering addon!\n");
		return -1;
	}
	
	al_register_event_source(event_queue, al_get_display_event_source(display)); //this allow us to quit the program using OS hotkey, X button, etc...
	al_register_event_source(event_queue, al_get_timer_event_source(timer)); //this allows the 1/FPS timer to call update function
	al_register_event_source(event_queue, al_get_mouse_event_source());
	
	al_clear_to_color(al_map_rgb(0,0,0));
	al_flip_display(); //this + above function to remove garbage that was in VRAM memory
	
	init_ball(); //move ball to center of the screen
	player1x = ballX; //move player 1 to center of screen
	player1y = SCREEN_H-PLAYER_H_HEIGHT; //move player 1 to botton of screen
	player2y = PLAYER_H_HEIGHT;
	
	al_start_timer(timer); //game start updating here
	
	while(1)
	{
		ALLEGRO_EVENT ev;
		al_wait_for_event(event_queue, &ev);
		
		if(ev.type == ALLEGRO_EVENT_TIMER) //this is our 1/FPS timer, thus it wants a game update
		{
			//move our ball
			ballX+=ballSpeedX;
			ballY+=ballSpeedY;
			if(ballX+BALL_SIZE_H > SCREEN_W)
			{
				ballX=SCREEN_W-BALL_SIZE_H; //we must move the ball enough to the condition to this become false, otherwise the ball get stuck
				ballSpeedX = -ballSpeedX;
				if(wooblespeed < 0) wooblespeed-=150; else wooblespeed+=150;
			}
			if(ballX-BALL_SIZE_H < 0)
			{
				ballX = BALL_SIZE_H;
				ballSpeedX = -ballSpeedX;
				if(wooblespeed < 0) wooblespeed-=150; else wooblespeed+=150; //this is just a little feedback effect, so the player can see a collision happened
			}
			if(ballY < 0 || ballY > SCREEN_H) //alright, someone made a point, but who?
			{
				sampleSpeed = 1+(float)(player1Score-player2Score)/10; //if human player is winning, play sounds in higher pitch, if losing, lower pitch.
				if(sampleSpeed < 0.15) sampleSpeed = 0.15; //if we let it go below that, the sound card might get bugs, and speakers might get damaged
				if(sampleSpeed > 3) sampleSpeed = 3; //if we let it go much above that, older people cannot hear it, also it is very annoying
				if(ballY < 0) //if this is true, the scorer is the human player
				{
					++player1Score; //add one point!
					aiMaxSpeed+=3; //make the game harder!
					al_play_sample(wonSample, 1.0, 0.0,sampleSpeed,ALLEGRO_PLAYMODE_ONCE,NULL); //feedback that a score was made.
				}
				else //oh no, the AI that scored!
				{
					++player2Score; //add one point to AI
					if(player2Score > player1Score + 3 && aiMaxSpeed > 2) aiMaxSpeed-=2; //if ai is kicking the crap out of player, make game easier!
					al_play_sample(lostSample, 1.0, 0.0,sampleSpeed,ALLEGRO_PLAYMODE_ONCE,NULL); //feedback that a score was made.
					
				}
				init_ball(); //send the ball to center of screen! Also resets its speed.
			}
			if(ballY-BALL_SIZE_H < PLAYER_H_HEIGHT*2+BLACK_LINE_WIDTH && (point_between_points(ballX-BALL_SIZE_H, player2x-PLAYER_H_WIDTH, player2x+PLAYER_H_WIDTH) || (point_between_points(ballX+BALL_SIZE_H, player2x-PLAYER_H_WIDTH, player2x+PLAYER_H_WIDTH))))
			{ //player2 hit the ball
				ballY = PLAYER_H_HEIGHT*2+BALL_SIZE_H+BLACK_LINE_WIDTH; //make ball don't get stuck
				ballSpeedY = -ballSpeedY; //return ball
				wooblespeed = 150; //add some feedback effect
				al_play_sample(bounceSample, 1.0, 0.0,1.0+sqrt(ballSpeedX*ballSpeedX+ballSpeedY*ballSpeedY)/30,ALLEGRO_PLAYMODE_ONCE,NULL);
				//the lines below make the ball go faster, the slower it is, the more likely this to happen, a extra wooble indicate this to the player
				if(rand()%80>abs(ballSpeedY)+abs(3*ballSpeedX)) ++ballSpeedY, wooblespeed+=200;
				if(rand()%80>abs(ballSpeedY)+abs(3*ballSpeedX))
				{
					if(ballSpeedX > 0) ++ballSpeedX; else --ballSpeedX;
					wooblespeed+=200;
				}
			}
			if(ballY+BALL_SIZE_H > SCREEN_H-PLAYER_H_HEIGHT*2-BLACK_LINE_WIDTH && (point_between_points(ballX-BALL_SIZE_H, player1x-PLAYER_H_WIDTH, player1x+PLAYER_H_WIDTH) || (point_between_points(ballX+BALL_SIZE_H, player1x-PLAYER_H_WIDTH, player1x+PLAYER_H_WIDTH))))
			{ //same as above, but for player1, I separated the two players to improve code readability
				ballY = SCREEN_H-PLAYER_H_HEIGHT*2-BALL_SIZE_H-BLACK_LINE_WIDTH;
				ballSpeedY = -ballSpeedY;
				wooblespeed = 150;
				al_play_sample(bounceSample, 1.0, 0.0,1.0+sqrt(ballSpeedX*ballSpeedX+ballSpeedY*ballSpeedY)/30,ALLEGRO_PLAYMODE_ONCE,NULL);
				if(rand()%80>abs(ballSpeedY)+abs(3*ballSpeedX)) --ballSpeedY, wooblespeed+=200;
				if(rand()%80>abs(ballSpeedY)+abs(3*ballSpeedX))
				{
					if(ballSpeedX > 0) ++ballSpeedX; else --ballSpeedX;
					wooblespeed+=200;
				}
			}
			
			delta = player2x-ballX; //this is the distance between AI player center and ball center.
			if(delta<-aiMaxSpeed) delta = -aiMaxSpeed; //without this line, or the below one, the game is impossible to win.
			if(delta>aiMaxSpeed) delta = aiMaxSpeed; //this prevents the AI from just teleporting to the ball all times.
			player2x-=delta; //move AI player toward the ball, maximum speed OR exactly aligned with the ball if it is close enough.
			if(player2x < LEFT_BUFFER) player2x = LEFT_BUFFER;
			if(player2x > RIGHT_BUFFER) player2x = RIGHT_BUFFER;
			redraw = true;
			
			//here we calculate the ball size feedback effect, note this is purely visual, don't affect the game rules.
			//if you are wondering where the equations came from, this is based on the "spring" equation
			wooblespeed+=-20*wooble-wooblespeed/15; //speed = speed+acceleration, acceleration = force/mass, mass = 1, acceleration = force, force = k*distance, the /15 is to simulate simple friction and energy loss and avoid infinite woobling.
			wooble+=wooblespeed/100; //position = position + speed. the /100 here is because of the previous numbers were inflated to allow integer division and whatnot.
			if(wooble == -1 && abs(wooblespeed) < 100) wooble = 0, wooblespeed = 0; //stop woobling if there  is not much to wooble anymore.
		}
		else if(ev.type == ALLEGRO_EVENT_DISPLAY_CLOSE) //someone clicked X button or used a hotkey, or got a TERM signal from the OS.
		{
			break;
		}
		else if(ev.type == ALLEGRO_EVENT_MOUSE_AXES || ev.type == ALLEGRO_EVENT_MOUSE_ENTER_DISPLAY) //player used the mouse
		{
			//first we take the mouse coordinates, that don't respect our visual transformation, and transform them manually
			ev.mouse.x=ev.mouse.x/scalex-translationx/scalex;
			
			//now we update the player
			player1x = ev.mouse.x;
			if(player1x < LEFT_BUFFER) player1x = LEFT_BUFFER;
			if(player1x > RIGHT_BUFFER) player1x = RIGHT_BUFFER;
			redraw = true;
		}
		else if(ev.type == ALLEGRO_EVENT_MOUSE_BUTTON_UP) //quit if the player clicks anywhere on the screen
		{
			break;
		}
		
		if(redraw && al_is_event_queue_empty(event_queue)) //so, we have a "redraw" call, and nothing else is going on... time to draw screen
		{
			redraw = false;
			
			al_clear_to_color(al_map_rgb(255,255,255)); //set our screen to white, mondrian painting a its best
			
			//here we draw the black lines that frame the human player
			al_draw_filled_rectangle(player1x-PLAYER_H_WIDTH-BLACK_LINE_WIDTH, 0, player1x-PLAYER_H_WIDTH, SCREEN_H, al_map_rgb(0, 0, 0));
			al_draw_filled_rectangle(player1x+PLAYER_H_WIDTH, 0, player1x+PLAYER_H_WIDTH+BLACK_LINE_WIDTH, SCREEN_H, al_map_rgb(0, 0, 0));
			al_draw_filled_rectangle(0, player1y-PLAYER_H_HEIGHT-BLACK_LINE_WIDTH, SCREEN_W, player1y-PLAYER_H_HEIGHT, al_map_rgb(0, 0, 0));
			
			//now the black lines that frame the AI player
			al_draw_filled_rectangle(player2x-PLAYER_H_WIDTH-BLACK_LINE_WIDTH, 0, player2x-PLAYER_H_WIDTH, SCREEN_H, al_map_rgb(0, 0, 0));
			al_draw_filled_rectangle(player2x+PLAYER_H_WIDTH, 0, player2x+PLAYER_H_WIDTH+BLACK_LINE_WIDTH, SCREEN_H, al_map_rgb(0, 0, 0));
			al_draw_filled_rectangle(0, player2y+PLAYER_H_HEIGHT, SCREEN_W, player2y+PLAYER_H_HEIGHT+BLACK_LINE_WIDTH, al_map_rgb(0, 0, 0));
			
			//we calculate the current visual size of the ball
			ballx1 = ballX-BALL_SIZE_H-wooble;
			bally1 = ballY-BALL_SIZE_H-wooble;
			ballx2 = ballX+BALL_SIZE_H+wooble;
			bally2 = ballY+BALL_SIZE_H+wooble;
			
			//now we draw the black lines that frame the ball
			al_draw_filled_rectangle(ballx1-BLACK_LINE_WIDTH, 0, ballx1, SCREEN_H, al_map_rgb(0, 0, 0));
			al_draw_filled_rectangle(ballx2, 0, ballx2+BLACK_LINE_WIDTH, SCREEN_H, al_map_rgb(0, 0, 0));
			al_draw_filled_rectangle(0, bally1-BLACK_LINE_WIDTH, SCREEN_W, bally1, al_map_rgb(0, 0, 0));
			al_draw_filled_rectangle(0, bally2, SCREEN_W, bally2+BLACK_LINE_WIDTH, al_map_rgb(0, 0, 0));
			
			//we drew the lines before this, because in Piet Mondrian paintings they are always behind other things, never on top of them.
			//again following Piet Mondrian rules, we draw our elements using primary colours, blue and red for players, yellow for ball.
			//mind you, these are the primary colours when you add paint to a surface, and it is a approximation based on historic basis.
			//for curiosity, it was the first experiments with prisms that resulted into people concluding that the colours were Red, Blue and Yellow
			al_draw_filled_rectangle(player1x-PLAYER_H_WIDTH, player1y-PLAYER_H_HEIGHT, player1x+PLAYER_H_WIDTH, player1y+PLAYER_H_HEIGHT, al_map_rgb(255, 0, 0));
			al_draw_filled_rectangle(ballx1, bally1, ballx2, bally2, al_map_rgb(255, 255, 0));
			al_draw_filled_rectangle(player2x-PLAYER_H_WIDTH, player2y-PLAYER_H_HEIGHT, player2x+PLAYER_H_WIDTH, player2y+PLAYER_H_HEIGHT, al_map_rgb(0, 0, 255));
			
			//Now we draw our "frame", this mean four black lines around the screen, only visisble if the person has a screen bigger than 600x600
			al_draw_filled_rectangle(-BLACK_LINE_WIDTH, -BLACK_LINE_WIDTH, SCREEN_W+BLACK_LINE_WIDTH, 0, al_map_rgb(0, 0, 0));
			al_draw_filled_rectangle(-BLACK_LINE_WIDTH, SCREEN_H, SCREEN_W+BLACK_LINE_WIDTH, SCREEN_H+BLACK_LINE_WIDTH, al_map_rgb(0, 0, 0));
			al_draw_filled_rectangle(-BLACK_LINE_WIDTH, 0, 0, SCREEN_H, al_map_rgb(0, 0, 0));
			al_draw_filled_rectangle(SCREEN_W, 0, SCREEN_W+BLACK_LINE_WIDTH, SCREEN_H, al_map_rgb(0, 0, 0));
			
			//and to complete the "frame", we erase using white "paint" any parts of the ball that bleed outside the screen upon colliding with top or bottom
			al_draw_filled_rectangle(-BLACK_LINE_WIDTH, -BALL_SIZE_H-BLACK_LINE_WIDTH, SCREEN_W+BLACK_LINE_WIDTH, -BLACK_LINE_WIDTH, al_map_rgb(255, 255, 255));
			al_draw_filled_rectangle(-BLACK_LINE_WIDTH, SCREEN_H+BLACK_LINE_WIDTH, SCREEN_W+BLACK_LINE_WIDTH, SCREEN_H+BLACK_LINE_WIDTH+BALL_SIZE_H, al_map_rgb(255, 255, 255));
			
			//now we flip the display. Mind you, if vsync is off, it might flip the display mid-screen update, and result into some ugly tearing
			al_flip_display();
		}
	}
	
	al_destroy_timer(timer);
	al_destroy_display(display);
	al_destroy_event_queue(event_queue);
	
	printf("Player1 Score: %d Player2 Score: %d", player1Score, player2Score);

	return 0;
}
