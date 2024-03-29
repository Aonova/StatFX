/* Easing functions, adapted from nicolausYes on GitHub:
 * https://github.com/nicolausYes/easing-functions/tree/master
 * ----------
 * usage:
 * auto easingFunction = getEasingFunction( easing_functions::{name} );
 * progress = easingFunction( {float linear input between 0 and 1} ); // returns eased float between 0 and 1
*/
#include <cmath>
#include <map>
#ifndef PI
#define PI 3.1415926545
#endif
#include <string>

namespace easing
{
    enum easing_functions
    {
        EaseInSine,
        EaseOutSine,
        EaseInOutSine,
        EaseInQuad,
        EaseOutQuad,
        EaseInOutQuad,
        EaseInCubic,
        EaseOutCubic,
        EaseInOutCubic,
        EaseInQuart,
        EaseOutQuart,
        EaseInOutQuart,
        EaseInQuint,
        EaseOutQuint,
        EaseInOutQuint,
        EaseInExpo,
        EaseOutExpo,
        EaseInOutExpo,
        EaseInCirc,
        EaseOutCirc,
        EaseInOutCirc,
        EaseInBack,
        EaseOutBack,
        EaseInOutBack,
        EaseInElastic,
        EaseOutElastic,
        EaseInOutElastic,
        EaseInBounce,
        EaseOutBounce,
        EaseInOutBounce
    };

    typedef double(*easingFunction)(double);

    easingFunction getEasingFunction( easing_functions function );

    double easeInSine( double t ) {
        return sin( 1.5707963 * t );
    }

    double easeOutSine( double t ) {
        return 1 + sin( 1.5707963 * (--t) );
    }

    double easeInOutSine( double t ) {
        return 0.5 * (1 + sin( 3.1415926 * (t - 0.5) ) );
    }

    double easeInQuad( double t ) {
        return t * t;
    }

    double easeOutQuad( double t ) {
        return t * (2 - t);
    }

    double easeInOutQuad( double t ) {
        return t < 0.5 ? 2 * t * t : t * (4 - 2 * t) - 1;
    }

    double easeInCubic( double t ) {
        return t * t * t;
    }

    double easeOutCubic( double t ) {
        return 1 + (--t) * t * t;
    }

    double easeInOutCubic( double t ) {
        return t < 0.5 ? 4 * t * t * t : 1 + (--t) * (2 * (--t)) * (2 * t);
    }

    double easeInQuart( double t ) {
        t *= t;
        return t * t;
    }

    double easeOutQuart( double t ) {
        t = (--t) * t;
        return 1 - t * t;
    }

    double easeInOutQuart( double t ) {
        if( t < 0.5 ) {
            t *= t;
            return 8 * t * t;
        } else {
            t = (--t) * t;
            return 1 - 8 * t * t;
        }
    }

    double easeInQuint( double t ) {
        double t2 = t * t;
        return t * t2 * t2;
    }

    double easeOutQuint( double t ) {
        double t2 = (--t) * t;
        return 1 + t * t2 * t2;
    }

    double easeInOutQuint( double t ) {
        double t2;
        if( t < 0.5 ) {
            t2 = t * t;
            return 16 * t * t2 * t2;
        } else {
            t2 = (--t) * t;
            return 1 + 16 * t * t2 * t2;
        }
    }

    double easeInExpo( double t ) {
        return (pow( 2, 8 * t ) - 1) / 255;
    }

    double easeOutExpo( double t ) {
        return 1 - pow( 2, -8 * t );
    }

    double easeInOutExpo( double t ) {
        if( t < 0.5 ) {
            return (pow( 2, 16 * t ) - 1) / 510;
        } else {
            return 1 - 0.5 * pow( 2, -16 * (t - 0.5) );
        }
    }

    double easeInCirc( double t ) {
        return 1 - sqrt( 1 - t );
    }

    double easeOutCirc( double t ) {
        return sqrt( t );
    }

    double easeInOutCirc( double t ) {
        if( t < 0.5 ) {
            return (1 - sqrt( 1 - 2 * t )) * 0.5;
        } else {
            return (1 + sqrt( 2 * t - 1 )) * 0.5;
        }
    }

    double easeInBack( double t ) {
        return t * t * (2.70158 * t - 1.70158);
    }

    double easeOutBack( double t ) {
        return 1 + (--t) * t * (2.70158 * t + 1.70158);
    }

    double easeInOutBack( double t ) {
        if( t < 0.5 ) {
            return t * t * (7 * t - 2.5) * 2;
        } else {
            return 1 + (--t) * t * 2 * (7 * t + 2.5);
        }
    }

    double easeInElastic( double t ) {
        double t2 = t * t;
        return t2 * t2 * sin( t * PI * 4.5 );
    }

    double easeOutElastic( double t ) {
        double t2 = (t - 1) * (t - 1);
        return 1 - t2 * t2 * cos( t * PI * 4.5 );
    }

    double easeInOutElastic( double t ) {
        double t2;
        if( t < 0.45 ) {
            t2 = t * t;
            return 8 * t2 * t2 * sin( t * PI * 9 );
        } else if( t < 0.55 ) {
            return 0.5 + 0.75 * sin( t * PI * 4 );
        } else {
            t2 = (t - 1) * (t - 1);
            return 1 - 8 * t2 * t2 * sin( t * PI * 9 );
        }
    }

    double easeInBounce( double t ) {
        return pow( 2, 6 * (t - 1) ) * abs( sin( t * PI * 3.5 ) );
    }

    double easeOutBounce( double t ) {
        return 1 - pow( 2, -6 * t ) * abs( cos( t * PI * 3.5 ) );
    }

    double easeInOutBounce( double t ) {
        if( t < 0.5 ) {
            return 8 * pow( 2, 8 * (t - 1) ) * abs( sin( t * PI * 7 ) );
        } else {
            return 1 - 8 * pow( 2, -8 * t ) * abs( sin( t * PI * 7 ) );
        }
    }

    double linear( double t ) {
        return t;
    }

    // get easing function from string name. See https://easings.net/ for valid names (case insensitive). Returns linear if unrecognized
    easingFunction getEasingFunctionString( std::string name ) {
        std::map< std::string, easing_functions > easingStrToFunc = {
            {"easeinsine", EaseInSine}, {"sine", EaseInSine}, {"sin(x)", EaseInSine}, {"1", EaseInSine},
            {"easeoutsine", EaseOutSine},
            {"easeinoutsine", EaseInOutSine},
            {"easeinquad", EaseInQuad}, {"quad", EaseInQuad}, {"x^2", EaseInQuad}, {"quadratic", EaseInQuad}, {"2", EaseInQuad},
            {"easeoutquad", EaseOutQuad},
            {"easeinoutquad", EaseInOutQuad},
            {"easeincubic", EaseInCubic}, {"cubic", EaseInCubic}, {"x^3", EaseInCubic}, {"cubed", EaseInCubic}, {"3", EaseInCubic},
            {"easeoutcubic", EaseOutCubic},
            {"easeinoutcubic", EaseInOutCubic},
            {"easeinquart", EaseInQuart}, {"quart", EaseInQuart}, {"x^4", EaseInQuart}, {"4", EaseInQuart},
            {"easeoutquart", EaseOutQuart},
            {"easeinoutquart", EaseInOutQuart},
            {"easeinquint", EaseInQuint}, {"quint", EaseInQuint}, {"x^5", EaseInQuint}, {"5", EaseInQuint},
            {"easeoutquint", EaseOutQuint},
            {"easeinoutquint", EaseInOutQuint},
            {"easeinexpo", EaseInExpo}, {"expo", EaseInExpo}, {"2^x", EaseInExpo}, {"6", EaseInExpo},
            {"easeoutexpo", EaseOutExpo},
            {"easeinoutexpo", EaseInOutExpo},
            {"easeincirc", EaseInCirc}, {"circ", EaseInCirc}, {"circle", EaseInCirc}, {"7", EaseInCirc},
            {"easeoutcirc", EaseOutCirc},
            {"easeinoutcirc", EaseInOutCirc},
            {"easeinback", EaseInBack}, {"back", EaseInBack},
            {"easeoutback", EaseOutBack},
            {"easeinoutback", EaseInOutBack},
            {"easeinelastic", EaseInElastic}, {"elastic", EaseInElastic},
            {"easeoutelastic", EaseOutElastic},
            {"easeinoutelastic", EaseInOutElastic},
            {"easeinbounce", EaseInBounce}, {"bounce", EaseInBounce},
            {"easeoutbounce", EaseOutBounce},
            {"easeinoutbounce", EaseInOutBounce}
        };
        // lowercase the name, making the lookup case insensitive
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);
        if (!easingStrToFunc.contains(name)) { return linear; }
        return getEasingFunction( easingStrToFunc.at(name) );
    }

    const char* getStringEasingFunction( easingFunction function ) {
        // return string name of passed function
        if (function == easeInSine) { return "easeInSine"; }
        else if (function == easeOutSine) { return "easeOutSine"; }
        else if (function == easeInOutSine) { return "easeInOutSine"; }
        else if (function == easeInQuad) { return "easeInQuad"; }
        else if (function == easeOutQuad) { return "easeOutQuad"; }
        else if (function == easeInOutQuad) { return "easeInOutQuad"; }
        else if (function == easeInCubic) { return "easeInCubic"; }
        else if (function == easeOutCubic) { return "easeOutCubic"; }
        else if (function == easeInOutCubic) { return "easeInOutCubic"; }
        else if (function == easeInQuart) { return "easeInQuart"; }
        else if (function == easeOutQuart) { return "easeOutQuart"; }
        else if (function == easeInOutQuart) { return "easeInOutQuart"; }
        else if (function == easeInQuint) { return "easeInQuint"; }
        else if (function == easeOutQuint) { return "easeOutQuint"; }
        else if (function == easeInOutQuint) { return "easeInOutQuint"; }
        else if (function == easeInExpo) { return "easeInExpo"; }
        else if (function == easeOutExpo) { return "easeOutExpo"; }
        else if (function == easeInOutExpo) { return "easeInOutExpo"; }
        else if (function == easeInCirc) { return "easeInCirc"; }
        else if (function == easeOutCirc) { return "easeOutCirc"; }
        else if (function == easeInOutCirc) { return "easeInOutCirc"; }
        else if (function == easeInBack) { return "easeInBack"; }
        else if (function == easeOutBack) { return "easeOutBack"; }
        else if (function == easeInOutBack) { return "easeInOutBack"; }
        else if (function == easeInElastic) { return "easeInElastic"; }
        else if (function == easeOutElastic) { return "easeOutElastic"; }
        else if (function == easeInOutElastic) { return "easeInOutElastic"; }
        else if (function == easeInBounce) { return "easeInBounce"; }
        else if (function == easeOutBounce) { return "easeOutBounce"; }
        else if (function == easeInOutBounce) { return "easeInOutBounce"; }
        else { return "linear"; }

    }

    easingFunction getEasingFunction( easing_functions function )
    {
        static std::map< easing_functions, easingFunction > easingFunctions;
        if( easingFunctions.empty() )
        {
            easingFunctions.insert( std::make_pair( EaseInSine, 	easeInSine ) );
            easingFunctions.insert( std::make_pair( EaseOutSine, 	easeOutSine ) );
            easingFunctions.insert( std::make_pair( EaseInOutSine, 	easeInOutSine ) );
            easingFunctions.insert( std::make_pair( EaseInQuad, 	easeInQuad ) );
            easingFunctions.insert( std::make_pair( EaseOutQuad, 	easeOutQuad ) );
            easingFunctions.insert( std::make_pair( EaseInOutQuad, 	easeInOutQuad ) );
            easingFunctions.insert( std::make_pair( EaseInCubic, 	easeInCubic ) );
            easingFunctions.insert( std::make_pair( EaseOutCubic, 	easeOutCubic ) );
            easingFunctions.insert( std::make_pair( EaseInOutCubic, easeInOutCubic ) );
            easingFunctions.insert( std::make_pair( EaseInQuart, 	easeInQuart ) );
            easingFunctions.insert( std::make_pair( EaseOutQuart, 	easeOutQuart ) );
            easingFunctions.insert( std::make_pair( EaseInOutQuart, easeInOutQuart) );
            easingFunctions.insert( std::make_pair( EaseInQuint, 	easeInQuint ) );
            easingFunctions.insert( std::make_pair( EaseOutQuint, 	easeOutQuint ) );
            easingFunctions.insert( std::make_pair( EaseInOutQuint, easeInOutQuint ) );
            easingFunctions.insert( std::make_pair( EaseInExpo, 	easeInExpo ) );
            easingFunctions.insert( std::make_pair( EaseOutExpo, 	easeOutExpo ) );
            easingFunctions.insert( std::make_pair( EaseInOutExpo,	easeInOutExpo ) );
            easingFunctions.insert( std::make_pair( EaseInCirc, 	easeInCirc ) );
            easingFunctions.insert( std::make_pair( EaseOutCirc, 	easeOutCirc ) );
            easingFunctions.insert( std::make_pair( EaseInOutCirc,	easeInOutCirc ) );
            easingFunctions.insert( std::make_pair( EaseInBack, 	easeInBack ) );
            easingFunctions.insert( std::make_pair( EaseOutBack, 	easeOutBack ) );
            easingFunctions.insert( std::make_pair( EaseInOutBack,	easeInOutBack ) );
            easingFunctions.insert( std::make_pair( EaseInElastic, 	easeInElastic ) );
            easingFunctions.insert( std::make_pair( EaseOutElastic, easeOutElastic ) );
            easingFunctions.insert( std::make_pair( EaseInOutElastic, easeInOutElastic ) );
            easingFunctions.insert( std::make_pair( EaseInBounce, 	easeInBounce ) );
            easingFunctions.insert( std::make_pair( EaseOutBounce, 	easeOutBounce ) );
            easingFunctions.insert( std::make_pair( EaseInOutBounce, easeInOutBounce ) );

        }

        auto it = easingFunctions.find( function );
        return it == easingFunctions.end() ? nullptr : it->second;
    }
}